/*
 * producer_consumer.c - Assignment 2
 * Classic producer-consumer problem using Pthreads condition variables.
 *
 * - One producer thread reads characters from "message.txt" one at a time
 *   and writes them into a circular buffer of BUFFER_SIZE = 5.
 * - One consumer thread reads characters from the buffer in the same order
 *   and prints them to stdout.
 *
 * Synchronization:
 *   mutex      - protects the shared buffer state
 *   not_full   - signaled when the buffer has room (producer waits on this)
 *   not_empty  - signaled when the buffer has data (consumer waits on this)
 *
 * Compile: gcc producer_consumer.c -o producer_consumer -lpthread
 * Run:     ./producer_consumer
 * Input:   message.txt
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 5   // circular queue capacity

// ---- shared circular buffer ----
char buffer[BUFFER_SIZE];
int  in    = 0;   // next write slot
int  out   = 0;   // next read slot
int  count = 0;   // number of items currently in buffer

// ---- synchronization primitives ----
pthread_mutex_t mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  not_full  = PTHREAD_COND_INITIALIZER;  // buffer not full
pthread_cond_t  not_empty = PTHREAD_COND_INITIALIZER;  // buffer not empty

// done is set to 1 by the producer after EOF
int done = 0;

// ------------------------------------------------------------------
void *producer(void *arg)
{
    FILE *fp = fopen("message.txt", "r");
    if (fp == NULL)
    {
        printf("ERROR: cannot open message.txt\n");
        pthread_exit(NULL);
    }

    int ch;
    while ((ch = fgetc(fp)) != EOF)
    {
        pthread_mutex_lock(&mutex);

        // wait while the buffer is full
        while (count == BUFFER_SIZE)
        {
            pthread_cond_wait(&not_full, &mutex);
        }

        // write character into circular buffer
        buffer[in] = (char)ch;
        in = (in + 1) % BUFFER_SIZE;
        count++;

        pthread_cond_signal(&not_empty);  // wake the consumer
        pthread_mutex_unlock(&mutex);
    }

    fclose(fp);

    // signal end-of-input so the consumer can exit
    pthread_mutex_lock(&mutex);
    done = 1;
    pthread_cond_signal(&not_empty);  // wake consumer if it is waiting
    pthread_mutex_unlock(&mutex);

    pthread_exit(NULL);
}

// ------------------------------------------------------------------
void *consumer(void *arg)
{
    while (1)
    {
        pthread_mutex_lock(&mutex);

        // wait while the buffer is empty and production is not finished
        while (count == 0 && !done)
        {
            pthread_cond_wait(&not_empty, &mutex);
        }

        // if buffer is empty and producer is done, exit
        if (count == 0 && done)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }

        // read one character from the circular buffer
        char ch = buffer[out];
        out = (out + 1) % BUFFER_SIZE;
        count--;

        pthread_cond_signal(&not_full);  // wake producer if it was waiting
        pthread_mutex_unlock(&mutex);

        putchar(ch);
    }

    putchar('\n');
    pthread_exit(NULL);
}

// ------------------------------------------------------------------
int main(int argc, char *argv[])
{
    pthread_t producer_tid, consumer_tid;

    pthread_create(&producer_tid, NULL, producer, NULL);
    pthread_create(&consumer_tid, NULL, consumer, NULL);

    pthread_join(producer_tid, NULL);
    pthread_join(consumer_tid, NULL);

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&not_full);
    pthread_cond_destroy(&not_empty);

    return 0;
}