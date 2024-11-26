#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "reman.h"

// Global variables
#define MAXR 1000
#define MAXT 100

int num_threads, num_resources, deadlock_avoidance;
int available[MAXR];                  // Tracks available resources
int allocated[MAXT][MAXR];            // Tracks resources allocated to threads
int requested[MAXT][MAXR];            // Tracks requests made by threads
int max_claim[MAXT][MAXR];            // Tracks maximum claims of threads
pthread_mutex_t lock;                 // Mutex for thread synchronization
pthread_cond_t cond[MAXT];            // Condition variables for blocking threads
int thread_status[MAXT] = {0};        // Tracks active threads
pthread_t thread_ids[MAXT];           // Maps pthread_self() to logical tid

// Helper function to find the tid of the calling thread
int find_tid() {
    pthread_t self = pthread_self();
    for (int i = 0; i < num_threads; i++) {
        if (pthread_equal(thread_ids[i], self)) {
            return i;
        }
    }
    return -1; // Thread not found
}

// Helper function: Check if the system is in a safe state
int is_safe_state() {
    int work[MAXR];
    int finish[MAXT] = {0};

    // Initialize work with available resources
    for (int i = 0; i < num_resources; i++) {
        work[i] = available[i];
    }

    // Try to find a sequence where all threads can finish
    int found = 1;
    while (found) {
        found = 0;
        for (int tid = 0; tid < num_threads; tid++) {
            if (!finish[tid]) {
                int can_finish = 1;
                for (int i = 0; i < num_resources; i++) {
                    if (requested[tid][i] > work[i]) {
                        can_finish = 0;
                        break;
                    }
                }
                if (can_finish) {
                    for (int i = 0; i < num_resources; i++) {
                        work[i] += allocated[tid][i];
                    }
                    finish[tid] = 1;
                    found = 1;
                }
            }
        }
    }

    // Check if all threads can finish
    for (int tid = 0; tid < num_threads; tid++) {
        if (!finish[tid]) return 0; // Not safe
    }
    return 1; // Safe state
}

// reman_init: Initializes the library
int reman_init(int t_count, int r_count, int avoid) {
    if (t_count > MAXT || r_count > MAXR) return -1; // Check for limits

    num_threads = t_count;
    num_resources = r_count;
    deadlock_avoidance = avoid;

    pthread_mutex_init(&lock, NULL);
    for (int i = 0; i < num_threads; i++) {
        pthread_cond_init(&cond[i], NULL);
    }

    for (int i = 0; i < num_resources; i++) {
        available[i] = 1; // Initialize all resources as available
    }

    // Initialize resource tracking arrays
    for (int i = 0; i < num_threads; i++) {
        for (int j = 0; j < num_resources; j++) {
            allocated[i][j] = 0;
            requested[i][j] = 0;
            max_claim[i][j] = 0;
        }
    }

    return 0;
}

// reman_connect: Registers a thread in the library
int reman_connect(int tid) {
    if (tid < 0 || tid >= num_threads) return -1;

    pthread_mutex_lock(&lock);
    thread_ids[tid] = pthread_self(); // Map this thread's tid to its pthread_self()
    thread_status[tid] = 1;           // Mark thread as active
    pthread_mutex_unlock(&lock);

    return 0;
}

// reman_disconnect: Unregisters a thread
int reman_disconnect() {
    pthread_mutex_lock(&lock);
    int tid = find_tid();
    if (tid == -1) {
        pthread_mutex_unlock(&lock);
        return -1; // Thread not registered
    }
    thread_status[tid] = 0; // Mark thread as inactive
    pthread_mutex_unlock(&lock);
    return 0;
}

// reman_claim: Sets a thread's maximum claim for resources
int reman_claim(int claim[]) {
    pthread_mutex_lock(&lock);
    int tid = find_tid();
    if (tid == -1) {
        pthread_mutex_unlock(&lock);
        return -1; // Thread not registered
    }

    // Set the max_claim for this thread
    for (int i = 0; i < num_resources; i++) {
        max_claim[tid][i] = claim[i];
    }

    pthread_mutex_unlock(&lock);
    return 0;
}

// reman_request: Handles resource requests by threads
int reman_request(int request[]) {
    pthread_mutex_lock(&lock);
    int tid = find_tid();
    if (tid == -1) {
        pthread_mutex_unlock(&lock);
        return -1; // Thread not registered
    }

    while (1) { // Loop to retry the request
        int can_allocate = 1;

        // Check if the request is valid
        for (int i = 0; i < num_resources; i++) {
            if (request[i] > available[i] || request[i] + allocated[tid][i] > max_claim[tid][i]) {
                can_allocate = 0;
                break;
            }
            requested[tid][i] = request[i];
        }

        if (can_allocate) {
            // Temporarily allocate resources
            for (int i = 0; i < num_resources; i++) {
                available[i] -= request[i];
                allocated[tid][i] += request[i];
                requested[tid][i] = 0;
            }

            // If deadlock avoidance is enabled, check for a safe state
            if (deadlock_avoidance && !is_safe_state()) {
                // Roll back allocation
                for (int i = 0; i < num_resources; i++) {
                    available[i] += request[i];
                    allocated[tid][i] -= request[i];
                    requested[tid][i] = request[i];
                }
                pthread_mutex_unlock(&lock);
                return -1; // Unsafe state
            }

            pthread_mutex_unlock(&lock);
            return 0; // Allocation successful
        } else {
            // Block the thread if resources are unavailable
            pthread_cond_wait(&cond[tid], &lock);
        }
    }
}

// reman_release: Releases resources held by a thread
int reman_release(int release[]) {
    pthread_mutex_lock(&lock);
    int tid = find_tid();
    if (tid == -1) {
        pthread_mutex_unlock(&lock);
        return -1; // Thread not registered
    }

    // Release resources
    for (int i = 0; i < num_resources; i++) {
        if (release[i] > allocated[tid][i]) {
            pthread_mutex_unlock(&lock);
            return -1; // Invalid release
        }
        available[i] += release[i];
        allocated[tid][i] -= release[i];
    }

    // Notify all waiting threads
    pthread_cond_broadcast(&cond[tid]);

    pthread_mutex_unlock(&lock);
    return 0;
}

// reman_detect: Detects deadlocks using a simple detection mechanism
int reman_detect() {
    pthread_mutex_lock(&lock);

    int deadlock_count = 0;
    for (int tid = 0; tid < num_threads; tid++) {
        int has_unmet_request = 0;
        for (int i = 0; i < num_resources; i++) {
            if (requested[tid][i] > 0) {
                has_unmet_request = 1;
                break;
            }
        }
        if (has_unmet_request) deadlock_count++;
    }

    pthread_mutex_unlock(&lock);
    return deadlock_count;
}

// reman_print: Prints the current state of resources
void reman_print(char title[]) {
    pthread_mutex_lock(&lock);

    printf("##########################\n");
    printf("%s\n", title);
    printf("##########################\n");

    printf("Resource Count: %d\n", num_resources);
    printf("Thread Count: %d\n", num_threads);

    printf("\nAvailable Resources:\n");
    for (int i = 0; i < num_resources; i++) {
        printf("R%d: %d ", i, available[i]);
    }
    printf("\n");

    printf("\nMaximum Claim:\n");
    for (int tid = 0; tid < num_threads; tid++) {
        printf("T%d: ", tid);
        for (int i = 0; i < num_resources; i++) {
            printf("%d ", max_claim[tid][i]);
        }
        printf("\n");
    }

    printf("\nAllocated Resources:\n");
    for (int tid = 0; tid < num_threads; tid++) {
        printf("T%d: ", tid);
        for (int i = 0; i < num_resources; i++) {
            printf("%d ", allocated[tid][i]);
        }
        printf("\n");
    }

    pthread_mutex_unlock(&lock);
}
