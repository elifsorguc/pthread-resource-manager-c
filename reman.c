#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>  // For time-related functions and structures
#include <errno.h> // For error codes like ETIMEDOUT
#include "reman.h"

// Global variables
#define MAXR 1000
#define MAXT 100

int num_threads, num_resources, deadlock_avoidance;
int available[MAXR];           // Tracks available resources
int allocated[MAXT][MAXR];     // Tracks resources allocated to threads
int requested[MAXT][MAXR];     // Tracks requests made by threads
int max_claim[MAXT][MAXR];     // Tracks maximum claims of threads
pthread_mutex_t lock;          // Mutex for thread synchronization
pthread_cond_t cond[MAXT];     // Condition variables for blocking threads
int thread_status[MAXT] = {0}; // Tracks active threads
pthread_t thread_ids[MAXT];    // Maps pthread_self() to logical tid

// Helper function to find the tid of the calling thread
int find_tid()
{
    pthread_t self = pthread_self();
    for (int i = 0; i < num_threads; i++)
    {
        if (pthread_equal(thread_ids[i], self))
        {
            return i;
        }
    }
    return -1; // Thread not found
}

// Helper function: Check if the system is in a safe state
int is_safe_state()
{
    int work[MAXR];
    int finish[MAXT] = {0};

    printf("[DEBUG] Checking if system is in a safe state...\n");

    // Initialize work with available resources
    for (int i = 0; i < num_resources; i++)
    {
        work[i] = available[i];
    }

    int found = 1;
    while (found)
    {
        found = 0;
        for (int tid = 0; tid < num_threads; tid++)
        {
            if (!finish[tid])
            {
                int can_finish = 1;
                for (int i = 0; i < num_resources; i++)
                {
                    if (requested[tid][i] > work[i])
                    {
                        can_finish = 0;
                        break;
                    }
                }
                if (can_finish)
                {
                    printf("[DEBUG] Thread %d can finish. Releasing its resources.\n", tid);
                    for (int i = 0; i < num_resources; i++)
                    {
                        work[i] += allocated[tid][i];
                    }
                    finish[tid] = 1;
                    found = 1;
                }
            }
        }
    }

    for (int tid = 0; tid < num_threads; tid++)
    {
        if (!finish[tid])
        {
            printf("[DEBUG] System is not in a safe state.\n");
            return 0;
        }
    }

    printf("[DEBUG] System is in a safe state.\n");
    return 1;
}

// reman_init: Initializes the library
int reman_init(int t_count, int r_count, int avoid)
{
    printf("[DEBUG] Initializing library with %d threads and %d resources. Deadlock Avoidance: %s\n",
           t_count, r_count, avoid ? "Enabled" : "Disabled");

    if (t_count > MAXT || r_count > MAXR)
        return -1;

    num_threads = t_count;
    num_resources = r_count;
    deadlock_avoidance = avoid;

    pthread_mutex_init(&lock, NULL);
    for (int i = 0; i < num_threads; i++)
    {
        pthread_cond_init(&cond[i], NULL);
    }

    for (int i = 0; i < num_resources; i++)
    {
        available[i] = 1;
    }

    for (int i = 0; i < num_threads; i++)
    {
        for (int j = 0; j < num_resources; j++)
        {
            allocated[i][j] = 0;
            requested[i][j] = 0;
            max_claim[i][j] = 0;
        }
    }

    return 0;
}

// reman_connect: Registers a thread in the library
int reman_connect(int tid)
{
    if (tid < 0 || tid >= num_threads)
        return -1;

    pthread_mutex_lock(&lock);
    thread_ids[tid] = pthread_self();
    thread_status[tid] = 1;
    printf("[DEBUG] Thread %d connected.\n", tid);
    pthread_mutex_unlock(&lock);

    return 0;
}

// reman_disconnect: Unregisters a thread
int reman_disconnect()
{
    pthread_mutex_lock(&lock);
    int tid = find_tid();
    if (tid == -1)
    {
        pthread_mutex_unlock(&lock);
        return -1;
    }

    printf("[DEBUG] Thread %d disconnected.\n", tid);
    thread_status[tid] = 0;
    pthread_mutex_unlock(&lock);
    return 0;
}

// reman_claim: Sets a thread's maximum claim for resources
int reman_claim(int claim[])
{
    pthread_mutex_lock(&lock);
    int tid = find_tid();
    if (tid == -1)
    {
        pthread_mutex_unlock(&lock);
        return -1;
    }

    printf("[DEBUG] Thread %d claims: ", tid);
    for (int i = 0; i < num_resources; i++)
    {
        max_claim[tid][i] = claim[i];
        printf("%d ", claim[i]);
    }
    printf("\n");

    pthread_mutex_unlock(&lock);
    return 0;
}

// reman_request: Handles resource requests by threads
int reman_request(int request[])
{
    pthread_mutex_lock(&lock);
    int tid = find_tid();
    if (tid == -1)
    {
        pthread_mutex_unlock(&lock);
        return -1;
    }

    printf("[DEBUG] Thread %d requests: ", tid);
    for (int i = 0; i < num_resources; i++)
    {
        printf("%d ", request[i]);
    }
    printf("\n");

    while (1)
    {
        int can_allocate = 1;

        // Validate the request
        for (int i = 0; i < num_resources; i++)
        {
            if (request[i] > available[i] || request[i] + allocated[tid][i] > max_claim[tid][i])
            {
                can_allocate = 0;
                break;
            }
            requested[tid][i] = request[i];
        }

        if (can_allocate)
        {
            printf("[DEBUG] Allocating resources to Thread %d.\n", tid);
            for (int i = 0; i < num_resources; i++)
            {
                available[i] -= request[i];
                allocated[tid][i] += request[i];
                requested[tid][i] = 0;
            }

            if (deadlock_avoidance && !is_safe_state())
            {
                printf("[DEBUG] Unsafe state detected. Rolling back allocation for Thread %d.\n", tid);
                for (int i = 0; i < num_resources; i++)
                {
                    available[i] += request[i];
                    allocated[tid][i] -= request[i];
                    requested[tid][i] = request[i];
                }
                pthread_mutex_unlock(&lock);
                return -1;
            }

            pthread_mutex_unlock(&lock);
            return 0;
        }
        else
        {
            printf("[DEBUG] Thread %d is waiting for resources. Current available: ", tid);
            for (int i = 0; i < num_resources; i++)
            {
                printf("%d ", available[i]);
            }
            printf("\n");

            pthread_cond_wait(&cond[tid], &lock); // Wait for a signal
            printf("[DEBUG] Thread %d woke up to retry its request.\n", tid);
        }
    }
}
int reman_release(int release[])
{
    pthread_mutex_lock(&lock);
    int tid = find_tid();
    if (tid == -1)
    {
        printf("[DEBUG] Error: Thread not found during release.\n");
        pthread_mutex_unlock(&lock);
        return -1;
    }

    printf("[DEBUG] Thread %d releasing resources: ", tid);
    for (int i = 0; i < num_resources; i++)
    {
        printf("%d ", release[i]);

        if (release[i] > allocated[tid][i])
        {
            printf("\n[DEBUG] Error: Thread %d trying to release more resources than allocated.\n", tid);
            pthread_mutex_unlock(&lock);
            return -1;
        }

        available[i] += release[i];
        allocated[tid][i] -= release[i];
    }
    printf("\n[DEBUG] Resources successfully released by Thread %d.\n", tid);

    // Notify all waiting threads to re-check their requests
    pthread_cond_broadcast(&cond[tid]);

    printf("[DEBUG] Updated available resources: ");
    for (int i = 0; i < num_resources; i++)
    {
        printf("%d ", available[i]);
    }
    printf("\n");

    pthread_mutex_unlock(&lock);
    return 0;
}

// reman_detect: Detects deadlocks using a simple detection mechanism
int reman_detect()
{
    pthread_mutex_lock(&lock);

    int deadlock_count = 0;

    // Check for threads stuck in a waiting state
    for (int tid = 0; tid < num_threads; tid++)
    {
        int is_stuck = 1; // Assume the thread is stuck
        for (int i = 0; i < num_resources; i++)
        {
            if (requested[tid][i] > 0) // Thread has an unmet request
            {
                is_stuck = 0;
                break;
            }
        }
        if (is_stuck && thread_status[tid])
        {
            deadlock_count++;
        }
    }

    if (deadlock_count > 0)
    {
        printf("[DEBUG] Deadlocks detected: %d\n", deadlock_count);

        // Notify all threads to retry their requests
        printf("[DEBUG] Broadcasting to all threads to retry requests.\n");
        for (int tid = 0; tid < num_threads; tid++)
        {
            pthread_cond_broadcast(&cond[tid]);
        }
    }

    pthread_mutex_unlock(&lock);
    return deadlock_count;
}

// reman_print: Prints the current state of resources
void reman_print(char title[])
{
    pthread_mutex_lock(&lock);

    printf("##########################\n");
    printf("%s\n", title);
    printf("##########################\n");

    printf("Resource Count: %d\n", num_resources);
    printf("Thread Count: %d\n", num_threads);

    printf("\nAvailable Resources:\n");
    for (int i = 0; i < num_resources; i++)
    {
        printf("R%d: %d ", i, available[i]);
    }
    printf("\n");

    printf("\nMaximum Claim:\n");
    for (int tid = 0; tid < num_threads; tid++)
    {
        printf("T%d: ", tid);
        for (int i = 0; i < num_resources; i++)
        {
            printf("%d ", max_claim[tid][i]);
        }
        printf("\n");
    }

    printf("\nAllocated Resources:\n");
    for (int tid = 0; tid < num_threads; tid++)
    {
        printf("T%d: ", tid);
        for (int i = 0; i < num_resources; i++)
        {
            printf("%d ", allocated[tid][i]);
        }
        printf("\n");
    }

    pthread_mutex_unlock(&lock);
}
