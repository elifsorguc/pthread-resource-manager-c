#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdarg.h>
#include "reman.h"

#define NUMR 5 // Number of resource types
#define NUMT 3 // Number of threads

int AVOID = 1;
int state_changed = 0; // Track if the state has changed

void pr(int tid, char astr[], int m, int r[])
{
    int i;
    printf("thread %d, %s, [", tid, astr);
    for (i = 0; i < m; ++i)
    {
        if (i == (m - 1))
            printf("%d", r[i]);
        else
            printf("%d,", r[i]);
    }
    printf("]\n");
}

void setarray(int r[], int m, ...)
{
    va_list valist;
    int i;

    va_start(valist, m);
    for (i = 0; i < m; i++)
    {
        r[i] = va_arg(valist, int);
    }
    va_end(valist);
    return;
}

void *threadfunc1(void *a)
{
    int tid;
    int request1[MAXR];
    int request2[MAXR];
    int claim[MAXR];

    tid = *((int *)a);
    reman_connect(tid);
    setarray(claim, NUMR, 1, 0, 1, 1, 0); // Claim resources
    reman_claim(claim);

    setarray(request1, NUMR, 1, 0, 0, 0, 0); // Request resources
    pr(tid, "REQ", NUMR, request1);
    if (reman_request(request1) == 0)
    {
        state_changed = 1; // Mark state as changed after allocation
        reman_print("State after request by Thread 0");
    }
    sleep(2);

    setarray(request2, NUMR, 0, 0, 1, 0, 0); // Request additional resources
    pr(tid, "REQ", NUMR, request2);
    if (reman_request(request2) == 0)
    {
        state_changed = 1; // Mark state as changed after allocation
        reman_print("State after additional request by Thread 0");
    }

    reman_release(request1);
    state_changed = 1;
    reman_print("State after releasing resources by Thread 0");
    reman_release(request2);

    reman_disconnect();
    pthread_exit(NULL);
}

void *threadfunc2(void *a)
{
    int tid;
    int request1[MAXR];
    int request2[MAXR];
    int claim[MAXR];

    tid = *((int *)a);
    reman_connect(tid);
    setarray(claim, NUMR, 0, 1, 1, 0, 1); // Claim resources
    reman_claim(claim);

    setarray(request1, NUMR, 0, 1, 0, 0, 0); // Request resources
    pr(tid, "REQ", NUMR, request1);
    if (reman_request(request1) == 0)
    {
        state_changed = 1; // Mark state as changed after allocation
        reman_print("State after request by Thread 1");
    }
    sleep(3);

    setarray(request2, NUMR, 0, 0, 0, 1, 0); // Request additional resources
    pr(tid, "REQ", NUMR, request2);
    if (reman_request(request2) == 0)
    {
        state_changed = 1; // Mark state as changed after allocation
        reman_print("State after additional request by Thread 1");
    }

    reman_release(request1);
    state_changed = 1;
    reman_print("State after releasing resources by Thread 1");
    reman_release(request2);

    reman_disconnect();
    pthread_exit(NULL);
}

void *threadfunc3(void *a)
{
    int tid;
    int request1[MAXR];
    int request2[MAXR];
    int claim[MAXR];

    tid = *((int *)a);
    reman_connect(tid);
    setarray(claim, NUMR, 0, 1, 0, 1, 1); // Claim resources
    reman_claim(claim);

    setarray(request1, NUMR, 0, 0, 0, 1, 0); // Request resources
    pr(tid, "REQ", NUMR, request1);
    if (reman_request(request1) == 0)
    {
        state_changed = 1; // Mark state as changed after allocation
        reman_print("State after request by Thread 2");
    }
    sleep(4);

    setarray(request2, NUMR, 0, 1, 0, 0, 1); // Request additional resources
    pr(tid, "REQ", NUMR, request2);
    if (reman_request(request2) == 0)
    {
        state_changed = 1; // Mark state as changed after allocation
        reman_print("State after additional request by Thread 2");
    }

    reman_release(request1);
    state_changed = 1;
    reman_print("State after releasing resources by Thread 2");
    reman_release(request2);

    reman_disconnect();
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    int i;
    int tids[NUMT];
    pthread_t threadArray[NUMT];
    int count;
    int ret;

    if (argc != 2)
    {
        printf("usage: ./app avoid_flag\n");
        exit(1);
    }

    AVOID = atoi(argv[1]);
    if (AVOID == 1)
        reman_init(NUMT, NUMR, 1);
    else
        reman_init(NUMT, NUMR, 0);

    // Create threads with different behaviors
    for (i = 0; i < NUMT; ++i)
    {
        tids[i] = i;
        pthread_create(&(threadArray[i]), NULL, (void *)((i == 0) ? threadfunc1 : (i == 1) ? threadfunc2
                                                                                           : threadfunc3),
                       (void *)&tids[i]);
    }

    count = 0;
    while (count < 20)
    {
        sleep(1);

        ret = reman_detect(); // Detect deadlock
        if (ret > 0)
        {
            printf("deadlock detected, count=%d\n", ret);
            reman_print("State after deadlock resolution");
            state_changed = 1; // Mark state as changed after resolving deadlock
        }
        count++;
    }

    if (ret == 0)
    {
        for (i = 0; i < NUMT; ++i)
        {
            pthread_join(threadArray[i], NULL);
            printf("joined\n");
        }
    }
}
