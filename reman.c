#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "reman.h"

// Global Definitions/Variables
int available[MAXR];        // Available resources
int max_claim[MAXT][MAXR];  // Maximum claim for each thread
int allocation[MAXT][MAXR]; // Current allocation for each thread
int request[MAXT][MAXR];    // Pending resource requests
int connected[MAXT];        // Tracks if threads are connected
int total_threads = 0;      // Total number of threads
int total_resources = 0;    // Total number of resources
int deadlock_avoidance = 0; // Flag for deadlock avoidance

pthread_mutex_t resource_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t resource_cond = PTHREAD_COND_INITIALIZER;

int reman_init(int t_count, int r_count, int avoid)
{
    if (t_count > MAXT || r_count > MAXR || t_count <= 0 || r_count <= 0)
    {
        return -1; // Invalid parameters
    }

    pthread_mutex_lock(&resource_mutex);

    total_threads = t_count;
    total_resources = r_count;
    deadlock_avoidance = avoid;

    for (int i = 0; i < r_count; i++)
    {
        available[i] = 1; // Initialize each resource as available
    }

    for (int i = 0; i < t_count; i++)
    {
        connected[i] = 0;
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
        return -1;
    }

    pthread_mutex_lock(&resource_mutex);
    connected[tid] = 1;
    pthread_mutex_unlock(&resource_mutex);
    return 0;
}

int reman_disconnect()
{
    pthread_mutex_lock(&resource_mutex);

    for (int i = 0; i < total_threads; i++)
    {
        if (connected[i])
        {
            for (int j = 0; j < total_resources; j++)
            {
                available[j] += allocation[i][j];
                allocation[i][j] = 0;
            }
            connected[i] = 0;
        }
    }

    pthread_cond_broadcast(&resource_cond);
    pthread_mutex_unlock(&resource_mutex);
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
            return -1; // Invalid claim
        }
    }

    for (int t = 0; t < total_threads; t++)
    {
        for (int r = 0; r < total_resources; r++)
        {
            max_claim[t][r] = claim[r];
        }
    }

    pthread_mutex_unlock(&resource_mutex);
    return 0;
}

int reman_request(int req[])
{
    pthread_mutex_lock(&resource_mutex);

    for (int i = 0; i < total_resources; i++)
    {
        if (req[i] < 0 || req[i] > 1 || req[i] > available[i])
        {
            pthread_mutex_unlock(&resource_mutex);
            return -1; // Invalid request
        }
    }

    for (int t = 0; t < total_threads; t++)
    {
        for (int r = 0; r < total_resources; r++)
        {
            allocation[t][r] += req[r];
            available[r] -= req[r];
        }
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
            return -1; // Invalid release
        }
        allocation[0][i] -= release[i];
        available[i] += release[i];
    }

    pthread_cond_broadcast(&resource_cond);
    pthread_mutex_unlock(&resource_mutex);
    return 0;
}

int reman_detect()
{
    pthread_mutex_lock(&resource_mutex);

    int finish[MAXT] = {0};
    int work[MAXR];
    for (int i = 0; i < total_resources; i++)
    {
        work[i] = available[i];
    }

    for (int i = 0; i < total_threads; i++)
    {
        if (!finish[i])
        {
            for (int j = 0; j < total_resources; j++)
            {
                if (request[i][j] <= work[j])
                {
                    work[j] += allocation[i][j];
                }
            }
            finish[i] = 1;
        }
    }

    pthread_mutex_unlock(&resource_mutex);
    return 0;
}

void reman_print(char title[])
{
    pthread_mutex_lock(&resource_mutex);
    printf("##########################\n");
    printf("%s\n", title);
    printf("Available Resources:\n");

    for (int i = 0; i < total_resources; i++)
    {
        printf("%d ", available[i]);
    }

    printf("\n##########################\n");
    pthread_mutex_unlock(&resource_mutex);
}
