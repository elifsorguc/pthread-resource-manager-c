#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include "reman.h"

#define MAXR 1000
#define MAXT 100

int num_threads, num_resources, deadlock_avoidance;
int available[MAXR];
int allocated[MAXT][MAXR];
int requested[MAXT][MAXR];
int max_claim[MAXT][MAXR];
pthread_mutex_t lock;
pthread_cond_t cond[MAXT];
int thread_status[MAXT] = {0};
pthread_t thread_ids[MAXT];

#define TIMEOUT 5 // Timeout for condition variable wait

int find_tid() {
    pthread_t self = pthread_self();
    for (int i = 0; i < num_threads; i++) {
        if (pthread_equal(thread_ids[i], self)) {
            return i;
        }
    }
    return -1;
}

int is_safe_state() {
    int work[MAXR];
    int finish[MAXT] = {0}; // Tracks whether each thread can finish

    printf("[DEBUG] Checking safe state...\n");

    // Initialize work array to represent currently available resources
    for (int i = 0; i < num_resources; i++) {
        work[i] = available[i];
    }

    // Debugging: Print initial work array
    printf("[DEBUG] Initial work array: ");
    for (int i = 0; i < num_resources; i++) {
        printf("%d ", work[i]);
    }
    printf("\n");

    int found; // Used to track progress in the loop
    do {
        found = 0; // Reset progress indicator for each pass

        for (int tid = 0; tid < num_threads; tid++) {
            if (!finish[tid]) { // Check if the thread has not yet finished
                int can_finish = 1;
                for (int i = 0; i < num_resources; i++) {
                    // Check if the thread's requested resources are <= available in work
                    if (requested[tid][i] > work[i]) {
                        can_finish = 0;
                        break;
                    }
                }

                if (can_finish) {
                    // Debugging: Print that the thread can finish
                    printf("[DEBUG] Thread %d can finish. Releasing resources.\n", tid);

                    // Add the thread's allocated resources back to work
                    for (int i = 0; i < num_resources; i++) {
                        work[i] += allocated[tid][i];
                    }
                    finish[tid] = 1; // Mark thread as finished
                    found = 1; // Indicate progress in this pass
                }
            }
        }
    } while (found); // Continue until no more threads can finish

    // Debugging: Final state of finish array
    printf("[DEBUG] Final finish array: ");
    for (int tid = 0; tid < num_threads; tid++) {
        printf("%d ", finish[tid]);
    }
    printf("\n");

    // Check if all threads can finish
    for (int tid = 0; tid < num_threads; tid++) {
        if (!finish[tid]) {
            printf("[DEBUG] System is in an unsafe state.\n");
            return 0; // Unsafe state
        }
    }

    printf("[DEBUG] System is in a safe state.\n");
    return 1; // Safe state
}



int reman_init(int t_count, int r_count, int avoid) {
    if (t_count > MAXT || r_count > MAXR)
        return -1;

    num_threads = t_count;
    num_resources = r_count;
    deadlock_avoidance = avoid;

    pthread_mutex_init(&lock, NULL);
    for (int i = 0; i < num_threads; i++) {
        pthread_cond_init(&cond[i], NULL);
    }

    for (int i = 0; i < num_resources; i++) {
        available[i] = 1;
    }

    for (int i = 0; i < num_threads; i++) {
        for (int j = 0; j < num_resources; j++) {
            allocated[i][j] = 0;
            requested[i][j] = 0;
            max_claim[i][j] = 0;
        }
    }
    printf("[DEBUG] Resource manager initialized with %d threads, %d resources, deadlock avoidance: %s.\n",
           t_count, r_count, avoid ? "Enabled" : "Disabled");
    return 0;
}

int reman_connect(int tid) {
    if (tid < 0 || tid >= num_threads)
        return -1;

    pthread_mutex_lock(&lock);
    thread_ids[tid] = pthread_self();
    thread_status[tid] = 1;
    pthread_mutex_unlock(&lock);
    return 0;
}

int reman_disconnect() {
    pthread_mutex_lock(&lock);
    int tid = find_tid();
    if (tid == -1) {
        pthread_mutex_unlock(&lock);
        return -1;
    }
    thread_status[tid] = 0;
    pthread_mutex_unlock(&lock);
    return 0;
}

int reman_claim(int claim[]) {
    pthread_mutex_lock(&lock);
    int tid = find_tid();
    if (tid == -1) {
        pthread_mutex_unlock(&lock);
        return -1;
    }
    for (int i = 0; i < num_resources; i++) {
        max_claim[tid][i] = claim[i];
    }
    pthread_mutex_unlock(&lock);
    return 0;
}

int reman_request(int request[]) {
    pthread_mutex_lock(&lock);
    int tid = find_tid();
    if (tid == -1) {
        pthread_mutex_unlock(&lock);
        return -1; // Invalid thread ID
    }

    printf("[DEBUG] Thread %d requesting resources: ", tid);
    for (int i = 0; i < num_resources; i++) {
        printf("%d ", request[i]);
    }
    printf("\n");

    // Check if the request exceeds the thread's maximum claim
    for (int i = 0; i < num_resources; i++) {
        if (request[i] + allocated[tid][i] > max_claim[tid][i]) {
            printf("[DEBUG] Request exceeds maximum claim for Thread %d.\n", tid);
            pthread_mutex_unlock(&lock);
            return -1; // Deny request
        }
    }

    // If deadlock avoidance is enabled, check for safety
    if (deadlock_avoidance) {
        // Temporarily allocate resources to check for safety
        for (int i = 0; i < num_resources; i++) {
            available[i] -= request[i];
            allocated[tid][i] += request[i];
        }

        if (!is_safe_state()) {
            // Rollback allocation if unsafe
            for (int i = 0; i < num_resources; i++) {
                available[i] += request[i];
                allocated[tid][i] -= request[i];
            }
            printf("[DEBUG] Unsafe state detected: Request denied for Thread %d.\n", tid);
            pthread_mutex_unlock(&lock);
            return -1; // Denied due to unsafe state
        }

        printf("[DEBUG] Request granted for Thread %d (safe state).\n", tid);
    } else {
        // For deadlock detection mode, grant the request without checking safety
        for (int i = 0; i < num_resources; i++) {
            available[i] -= request[i];
            allocated[tid][i] += request[i];
        }

        printf("[DEBUG] Request granted for Thread %d (no deadlock avoidance).\n", tid);
    }

    // Clear pending requests for the thread
    for (int i = 0; i < num_resources; i++) {
        requested[tid][i] = 0;
    }

    pthread_mutex_unlock(&lock);

    // For avoid = 0, detect and resolve deadlocks after granting the request
    if (!deadlock_avoidance) {
        reman_detect();
    }

    return 0; // Request granted
}





int reman_release(int release[]) {
    pthread_mutex_lock(&lock);
    int tid = find_tid();
    if (tid == -1) {
        pthread_mutex_unlock(&lock);
        return -1; // Invalid thread ID
    }

    for (int i = 0; i < num_resources; i++) {
        printf("%d ", release[i]);
    }
    printf("\n");

    for (int i = 0; i < num_resources; i++) {
        if (release[i] > allocated[tid][i]) {
            pthread_mutex_unlock(&lock);
            return -1; // Cannot release more than allocated
        }
        available[i] += release[i];
        allocated[tid][i] -= release[i];
    }

    printf("[DEBUG] Resources released by Thread %d.\n", tid);
    pthread_cond_broadcast(&cond[0]); // Notify all blocked threads
    pthread_mutex_unlock(&lock);
    return 0;
}



int reman_detect() {
    pthread_mutex_lock(&lock);

    int work[MAXR];
    int finish[MAXT] = {0};
    int deadlock_count = 0;

    // Initialize work array with available resources
    for (int i = 0; i < num_resources; i++) {
        work[i] = available[i];
    }

    printf("[DEBUG] Running deadlock detection...\n");

    // Mark threads with no allocated resources as "finished"
    for (int tid = 0; tid < num_threads; tid++) {
        int has_allocation = 0;
        for (int i = 0; i < num_resources; i++) {
            if (allocated[tid][i] > 0) {
                has_allocation = 1;
                break;
            }
        }
        if (!has_allocation) {
            finish[tid] = 1;
        }
    }

    // Try to finish threads in a simulated environment
    int found;
    do {
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
                    // Pretend thread finishes and releases its resources
                    for (int i = 0; i < num_resources; i++) {
                        work[i] += allocated[tid][i];
                    }
                    finish[tid] = 1;
                    found = 1;
                }
            }
        }
    } while (found);

    // Count deadlocked threads
    for (int tid = 0; tid < num_threads; tid++) {
        if (!finish[tid]) {
            deadlock_count++;
        }
    }

    if (deadlock_count > 0) {
        printf("[DEBUG] Deadlock detected with %d threads.\n", deadlock_count);

        // Preempt resources from the first detected deadlocked thread
        for (int tid = 0; tid < num_threads; tid++) {
            if (!finish[tid]) {
                printf("[DEBUG] Preempting resources from Thread %d.\n", tid);
                for (int i = 0; i < num_resources; i++) {
                    available[i] += allocated[tid][i];
                    allocated[tid][i] = 0;
                }
                break; // Preempt one thread at a time
            }
        }
    } else {
        printf("[DEBUG] No deadlocks detected.\n");
    }

    pthread_mutex_unlock(&lock);
    return deadlock_count;
}



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
