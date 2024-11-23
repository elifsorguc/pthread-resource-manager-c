#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "reman.h"

// Global Variables
int available[MAXR];        // Available resources
int max_claim[MAXT][MAXR];  // Maximum claims of threads
int allocation[MAXT][MAXR]; // Allocated resources
int request[MAXT][MAXR];    // Pending requests
int total_threads = 0;      // Total threads
int total_resources = 0;    // Total resource types
int deadlock_avoidance = 0; // Deadlock avoidance flag

pthread_mutex_t resource_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for synchronization
pthread_cond_t resource_cond = PTHREAD_COND_INITIALIZER;    // Condition variable

// Function Definitions

int reman_init(int t_count, int r_count, int avoid)
{
    // Initialize global variables
    if (t_count > MAXT || r_count > MAXR || t_count <= 0 || r_count <= 0)
    {
        return -1; // Invalid parameters
    }

    pthread_mutex_lock(&resource_mutex);
    total_threads = t_count;
    total_resources = r_count;
    deadlock_avoidance = avoid;

    // Initialize arrays
    for (int i = 0; i < r_count; i++)
    {
        available[i] = 1; // Assuming one instance per resource
    }

    for (int i = 0; i < t_count; i++)
    {
        for (int j = 0; j < r_count; j++)
        {
            max_claim[i][j] = 0;
            allocation[i][j] = 0;
            request[i][j] = 0;
        }
    }
    pthread_mutex_unlock(&resource_mutex);
    return 0;
}

int reman_connect(int tid)
{
    if (tid < 0 || tid >= total_threads)
    {
        return -1; // Invalid thread ID
    }
    // No additional initialization required for now
    return 0;
}

int reman_claim(int claim[])
{
    pthread_mutex_lock(&resource_mutex);

    for (int i = 0; i < total_resources; i++)
    {
        if (claim[i] < 0 || claim[i] > 1)
        {
            pthread_mutex_unlock(&resource_mutex);
            return -1; // Invalid claim array
        }
        max_claim[0][i] = claim[i]; // Store claim for the thread
    }

    pthread_mutex_unlock(&resource_mutex);
    return 0;
}

int reman_request(int request[])
{
    pthread_mutex_lock(&resource_mutex);

    // Check availability and deadlock safety
    for (int i = 0; i < total_resources; i++)
    {
        if (request[i] < 0 || request[i] > 1)
        {
            pthread_mutex_unlock(&resource_mutex);
            return -1; // Invalid request array
        }
        if (request[i] > available[i])
        {
            pthread_cond_wait(&resource_cond, &resource_mutex); // Wait for resources
        }
        allocation[0][i] += request[i];
        available[i] -= request[i];
    }

    pthread_mutex_unlock(&resource_mutex);
    return 0;
}

int reman_release(int release[])
{
    pthread_mutex_lock(&resource_mutex);

    for (int i = 0; i < total_resources; i++)
    {
        if (release[i] < 0 || release[i] > allocation[0][i])
        {
            pthread_mutex_unlock(&resource_mutex);
            return -1; // Invalid release array
        }
        allocation[0][i] -= release[i];
        available[i] += release[i];
    }

    pthread_cond_broadcast(&resource_cond); // Notify waiting threads
    pthread_mutex_unlock(&resource_mutex);
    return 0;
}

int reman_disconnect()
{
    // Clean-up or bookkeeping for the thread can be added here
    return 0;
}

int reman_detect()
{
    // Deadlock detection logic (e.g., graph traversal or cycle detection)
    return 0; // Placeholder: No deadlocks detected
}

void reman_print(char titlemsg[])
{
    pthread_mutex_lock(&resource_mutex);
    printf("##########################\n");
    printf("%s\n", titlemsg);
    printf("###########################\n");
    printf("Resource Count: %d\n", total_resources);
    printf("Thread Count: %d\n", total_threads);

    printf("Available Resources:\n");
    for (int i = 0; i < total_resources; i++)
    {
        printf("R%d: %d ", i, available[i]);
    }
    printf("\n");

    printf("Allocation Matrix:\n");
    for (int i = 0; i < total_threads; i++)
    {
        for (int j = 0; j < total_resources; j++)
        {
            printf("%d ", allocation[i][j]);
        }
        printf("\n");
    }
    printf("###########################\n");

    pthread_mutex_unlock(&resource_mutex);
}
