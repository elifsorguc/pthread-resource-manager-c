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
    int finish[MAXT] = {0};

    for (int i = 0; i < num_resources; i++) {
        work[i] = available[i];
    }

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

    for (int tid = 0; tid < num_threads; tid++) {
        if (!finish[tid]) {
            return 0;
        }
    }
    return 1;
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
        return -1;
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += TIMEOUT;

    while (1) {
        int can_allocate = 1;
        for (int i = 0; i < num_resources; i++) {
            if (request[i] > available[i] || request[i] + allocated[tid][i] > max_claim[tid][i]) {
                can_allocate = 0;
                break;
            }
            requested[tid][i] = request[i];
        }

        if (can_allocate) {
            for (int i = 0; i < num_resources; i++) {
                available[i] -= request[i];
                allocated[tid][i] += request[i];
                requested[tid][i] = 0;
            }

            if (deadlock_avoidance && !is_safe_state()) {
                for (int i = 0; i < num_resources; i++) {
                    available[i] += request[i];
                    allocated[tid][i] -= request[i];
                    requested[tid][i] = request[i];
                }
                pthread_mutex_unlock(&lock);
                return -1;
            }
            pthread_mutex_unlock(&lock);
            return 0;
        } else {
            int res = pthread_cond_timedwait(&cond[tid], &lock, &ts);
            if (res == ETIMEDOUT) {
                pthread_mutex_unlock(&lock);
                return -1; // Timeout, request denied
            }
        }
    }
}

int reman_release(int release[]) {
    pthread_mutex_lock(&lock);
    int tid = find_tid();
    if (tid == -1) {
        pthread_mutex_unlock(&lock);
        return -1;
    }

    for (int i = 0; i < num_resources; i++) {
        if (release[i] > allocated[tid][i]) {
            pthread_mutex_unlock(&lock);
            return -1;
        }
        available[i] += release[i];
        allocated[tid][i] -= release[i];
    }
    pthread_cond_broadcast(&cond[tid]); // Notify threads waiting for resources
    pthread_mutex_unlock(&lock);
    return 0;
}

int reman_detect() {
    pthread_mutex_lock(&lock);
    int deadlock_count = 0;

    for (int tid = 0; tid < num_threads; tid++) {
        int is_stuck = 1;
        for (int i = 0; i < num_resources; i++) {
            if (requested[tid][i] > 0) {
                is_stuck = 0;
                break;
            }
        }
        if (is_stuck && thread_status[tid]) {
            deadlock_count++;
        }
    }

    if (deadlock_count > 0) {
        for (int tid = 0; tid < num_threads; tid++) {
            pthread_cond_broadcast(&cond[tid]);
        }
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
