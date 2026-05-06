/*
 * my_list-forming.c - Assignment 3
 * Optimized version of list-forming.c.
 *
 * KEY OPTIMIZATIONS vs. the original:
 *
 * 1. LOCAL LIST BEFORE GLOBAL MERGE
 *    Each thread builds its own local list of K nodes entirely without
 *    touching the global lock. Only when all K nodes are ready does
 *    the thread acquire the global mutex once and splice the local list
 *    onto the global list. This reduces lock acquisitions from K per
 *    thread down to 1 per thread.
 *
 * 2. pthread_mutex_lock INSTEAD OF pthread_mutex_trylock
 *    The original used trylock in a busy-wait loop, burning CPU cycles
 *    while spinning. Using pthread_mutex_lock puts a blocked thread to
 *    sleep and lets the OS schedule other work, reducing wasted cycles
 *    especially when contention is high (many threads, large K).
 *
 * Compile: gcc my_list-forming.c -o my_list-forming -lpthread -D_GNU_SOURCE
 * Run:     ./my_list-forming <num_threads>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sched.h>

#define K 200  // number of nodes each thread generates

// ---- node and list structures (same as original) ----
struct Node
{
    int data;
    struct Node *next;
};

struct list
{
    struct Node *header;
    struct Node *tail;
};

pthread_mutex_t mutex_lock;
struct list *List;

// ------------------------------------------------------------------
void bind_thread_to_cpu(int cpuid)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpuid, &mask);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask))
    {
        fprintf(stderr, "sched_setaffinity failed\n");
        exit(EXIT_FAILURE);
    }
}

// ------------------------------------------------------------------
struct Node *generate_data_node(void)
{
    struct Node *ptr = (struct Node *)malloc(sizeof(struct Node));
    if (ptr != NULL)
    {
        ptr->next = NULL;
    }
    else
    {
        printf("Node allocation failed!\n");
    }
    return ptr;
}

/*
 * producer_thread (OPTIMIZED)
 *
 * Step 1 - Build a LOCAL list of K nodes (no locking needed).
 * Step 2 - Acquire the global mutex ONCE and splice the local list
 *          onto the tail of the global list, then release immediately.
 *
 * This cuts lock acquisitions from K -> 1 per thread.
 */
void *producer_thread(void *arg)
{
    bind_thread_to_cpu(*((int *)arg));

    // ---- local list state ----
    struct Node *local_head = NULL;
    struct Node *local_tail = NULL;
    int counter = 0;

    // Step 1: build the local list without any global locking
    while (counter < K)
    {
        struct Node *ptr = generate_data_node();
        if (ptr != NULL)
        {
            ptr->data = 1;  // generate data

            if (local_head == NULL)
            {
                local_head = local_tail = ptr;
            }
            else
            {
                local_tail->next = ptr;
                local_tail = ptr;
            }
            counter++;
        }
    }

    // Step 2: append local list to global list with a SINGLE lock/unlock
    if (local_head != NULL)
    {
        pthread_mutex_lock(&mutex_lock);

        if (List->header == NULL)
        {
            // global list was empty
            List->header = local_head;
            List->tail   = local_tail;
        }
        else
        {
            // append local list to the tail of the global list
            List->tail->next = local_head;
            List->tail       = local_tail;
        }

        pthread_mutex_unlock(&mutex_lock);
    }

    return NULL;
}

// ------------------------------------------------------------------
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s <num_threads>\n", argv[0]);
        return 1;
    }

    int i;
    int num_threads = atoi(argv[1]);
    int NUM_PROCS;
    int *cpu_array = NULL;
    struct Node *tmp, *next;
    struct timeval starttime, endtime;

    pthread_t producer[num_threads];

    NUM_PROCS = sysconf(_SC_NPROCESSORS_CONF);
    if (NUM_PROCS > 0)
    {
        cpu_array = (int *)malloc(NUM_PROCS * sizeof(int));
        if (cpu_array == NULL)
        {
            printf("Allocation failed!\n");
            exit(0);
        }
        for (i = 0; i < NUM_PROCS; i++)
        {
            cpu_array[i] = i;
        }
    }

    pthread_mutex_init(&mutex_lock, NULL);

    List = (struct list *)malloc(sizeof(struct list));
    if (List == NULL)
    {
        printf("List allocation failed!\n");
        exit(0);
    }
    List->header = List->tail = NULL;

    gettimeofday(&starttime, NULL);

    for (i = 0; i < num_threads; i++)
    {
        pthread_create(&producer[i], NULL, producer_thread, &cpu_array[i % NUM_PROCS]);
    }

    for (i = 0; i < num_threads; i++)
    {
        if (producer[i] != 0)
        {
            pthread_join(producer[i], NULL);
        }
    }

    gettimeofday(&endtime, NULL);

    // verify correctness: count nodes
    int node_count = 0;
    if (List->header != NULL)
    {
        next = tmp = List->header;
        while (tmp != NULL)
        {
            node_count++;
            next = tmp->next;
            free(tmp);
            tmp = next;
        }
    }
    printf("Total nodes in list: %d (expected %d)\n", node_count, num_threads * K);

    if (cpu_array != NULL)
    {
        free(cpu_array);
    }
    free(List);
    pthread_mutex_destroy(&mutex_lock);

    printf("Total run time is %ld microseconds.\n",
           (endtime.tv_sec  - starttime.tv_sec)  * 1000000 +
           (endtime.tv_usec - starttime.tv_usec));

    return 0;
}