/*
 * String_pthread.c - Assignment 1
 * Parallel substring matching using Pthreads.
 *
 * Each thread searches its portion of s1 for occurrences of s2.
 * String s1 is evenly divided into NUM_THREADS chunks (n1 % NUM_THREADS == 0).
 * After counting local matches, each thread adds its count to the
 * global total under a mutex lock.
 *
 * Compile: gcc String_pthread.c -o String_pthread -lpthread
 * Run:     ./String_pthread
 * Input:   strings.txt  (line 1 = s1, line 2 = s2)
 */

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define NUM_THREADS 4
#define MAX 1024

void *sub_string(void *);
int readf(FILE *fp);

int total = 0;       // global total count of matched substrings
int nlocal, n1, n2;  // nlocal = chunk size per thread
char *s1, *s2;
FILE *fp;
pthread_mutex_t total_lock;

// ------------------------------------------------------------------
int main(int argc, char *argv[])
{
    int i, rc;
    pthread_t threads[NUM_THREADS];

    pthread_mutex_init(&total_lock, NULL);
    readf(fp);

    // spawn threads, passing the thread index as argument
    for (i = 0; i < NUM_THREADS; i++)
    {
        rc = pthread_create(&threads[i], NULL, sub_string, (void *)(long)i);
        if (rc)
        {
            printf("ERROR: pthread_create() returned %d\n", rc);
            exit(-1);
        }
    }

    for (i = 0; i < NUM_THREADS; i++)
    {
        rc = pthread_join(threads[i], NULL);
        if (rc)
        {
            printf("ERROR: pthread_join() returned %d\n", rc);
            exit(-1);
        }
    }

    printf("The number of substrings is: %d\n", total);

    free(s1);
    free(s2);
    pthread_mutex_destroy(&total_lock);
    pthread_exit(0);
}

// ------------------------------------------------------------------
int readf(FILE *fp)
{
    if ((fp = fopen("strings.txt", "r")) == NULL)
    {
        printf("ERROR: can't open strings.txt!\n");
        return 0;
    }

    s1 = (char *)malloc(sizeof(char) * MAX);
    if (s1 == NULL)
    {
        printf("ERROR: Out of memory!\n");
        return -1;
    }

    s2 = (char *)malloc(sizeof(char) * MAX);
    if (s2 == NULL)
    {
        printf("ERROR: Out of memory!\n");
        return -1;
    }

    s1 = fgets(s1, MAX, fp);
    s2 = fgets(s2, MAX, fp);

    n1 = strlen(s1);       // length of s1 (includes newline)
    n2 = strlen(s2) - 1;   // length of s2 (strip newline)
    nlocal = n1 / NUM_THREADS;  // each thread handles nlocal positions

    if (s1 == NULL || s2 == NULL || n1 < n2)
    {
        return -1;
    }

    fclose(fp);
    return 0;
}

/*
 * sub_string() - thread function
 *
 * Thread tid searches starting positions [tid*nlocal, end] in s1
 * where end = min((tid+1)*nlocal - 1, n1-n2).
 * Because n2 < nlocal (guaranteed by the problem), no boundary
 * overlap is needed between adjacent threads.
 *
 * After counting locally, the count is added to `total` under a lock.
 */
void *sub_string(void *threadid)
{
    long tid = (long)threadid;

    int start = (int)(tid * nlocal);            // first starting position for this thread
    int end   = (int)((tid + 1) * nlocal - 1);  // last starting position for this thread

    // cap at the last valid starting index
    if (end > n1 - n2)
    {
        end = n1 - n2;
    }

    int local_count = 0;
    int i, j, k, count;

    for (i = start; i <= end; i++)
    {
        count = 0;
        for (j = i, k = 0; k < n2; j++, k++)
        {
            if (*(s1 + j) != *(s2 + k))
            {
                break;
            }
            else
            {
                count++;
            }

            if (count == n2)
            {
                local_count++;  // found a match
            }
        }
    }

    // safely add local count to global total
    pthread_mutex_lock(&total_lock);
    total += local_count;
    pthread_mutex_unlock(&total_lock);

    pthread_exit(0);
}