#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdarg.h>
#include "reman.h"

#define NUMR 3 // Number of resource types
#define NUMT 3 // Number of threads

int AVOID = 1;

void pr(int tid, char astr[], int m, int r[]) {
    int i;
    printf("thread %d, %s, [", tid, astr);
    for (i = 0; i < m; ++i) {
        if (i == (m - 1))
            printf("%d", r[i]);
        else
            printf("%d,", r[i]);
    }
    printf("]\n");
}

void setarray(int r[], int m, ...) {
    va_list valist;
    int i;
    va_start(valist, m);
    for (i = 0; i < m; i++) {
        r[i] = va_arg(valist, int);
    }
    va_end(valist);
    return;
}

void *threadfunc1(void *a) {
    int tid;
    int request1[MAXR];
    int claim[MAXR];

    tid = *((int *)a);
    reman_connect(tid);

    // Thread 0 claims maximum [1, 0, 1]
    setarray(claim, NUMR, 1, 0, 1);
    reman_claim(claim);

    // Thread 0 requests [1, 0, 0]
    setarray(request1, NUMR, 1, 0, 0);
    pr(tid, "REQ", NUMR, request1);
    reman_request(request1);

    sleep(3);

    // Thread 0 releases [1, 0, 0]
    reman_release(request1);
    reman_print("State after releasing request1 by Thread 0");

    reman_disconnect();
    pthread_exit(NULL);
}

void *threadfunc2(void *a) {
    int tid;
    int request1[MAXR];
    int claim[MAXR];

    tid = *((int *)a);
    reman_connect(tid);

    // Thread 1 claims maximum [0, 1, 1]
    setarray(claim, NUMR, 0, 1, 1);
    reman_claim(claim);

    // Thread 1 requests [0, 1, 0]
    setarray(request1, NUMR, 0, 1, 0);
    pr(tid, "REQ", NUMR, request1);
    reman_request(request1);

    sleep(3);

    // Thread 1 releases [0, 1, 0]
    reman_release(request1);
    reman_print("State after releasing request1 by Thread 1");

    reman_disconnect();
    pthread_exit(NULL);
}

void *threadfunc3(void *a) {
    int tid;
    int request1[MAXR];
    int claim[MAXR];

    tid = *((int *)a);
    reman_connect(tid);

    // Thread 2 claims maximum [1, 1, 0]
    setarray(claim, NUMR, 1, 1, 0);
    reman_claim(claim);

    // Thread 2 requests [0, 0, 1]
    setarray(request1, NUMR, 0, 0, 1);
    pr(tid, "REQ", NUMR, request1);
    reman_request(request1);

    sleep(3);

    // Thread 2 releases [0, 0, 1]
    reman_release(request1);
    reman_print("State after releasing request1 by Thread 2");

    reman_disconnect();
    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    int i;
    int tids[NUMT];
    pthread_t threadArray[NUMT];
    int count;
    int ret;

    if (argc != 2) {
        printf("usage: ./app avoid_flag\n");
        exit(1);
    }

    AVOID = atoi(argv[1]);
    reman_init(NUMT, NUMR, AVOID);

    // Create 3 threads
    for (i = 0; i < NUMT; i++) {
        tids[i] = i;
        pthread_create(&(threadArray[i]), NULL,
                       (i == 0) ? threadfunc1 :
                       (i == 1) ? threadfunc2 :
                                  threadfunc3,
                       (void *)&tids[i]);
    }

    count = 0;
    while (count < 10) {
        sleep(1);
        reman_print("The current state");
        ret = reman_detect();
        if (ret > 0) {
            printf("deadlock detected, count=%d\n", ret);
            reman_print("state after deadlock");
        }
        count++;
    }

    for (i = 0; i < NUMT; ++i) {
        pthread_join(threadArray[i], NULL);
        printf("joined\n");
    }
    return 0;
}