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
    setarray(claim, NUMR, 1, 1, 1, 0, 0); // Claim resources
    reman_claim(claim);

    setarray(request1, NUMR, 1, 0, 0, 0, 0); // Request resources
    pr(tid, "REQ", NUMR, request1);
    reman_request(request1);
    sleep(2);

    setarray(request2, NUMR, 0, 1, 0, 0, 0); // Request additional resources
    pr(tid, "REQ", NUMR, request2);
    reman_request(request2);

    reman_release(request1);
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
    setarray(claim, NUMR, 0, 1, 1, 1, 0); // Claim resources
    reman_claim(claim);

    setarray(request1, NUMR, 0, 1, 0, 0, 0); // Request resources
    pr(tid, "REQ", NUMR, request1);
    reman_request(request1);
    sleep(3);

    setarray(request2, NUMR, 0, 0, 1, 0, 0); // Request additional resources
    pr(tid, "REQ", NUMR, request2);
    reman_request(request2);

    reman_release(request1);
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
    setarray(claim, NUMR, 1, 0, 0, 1, 1); // Claim resources
    reman_claim(claim);

    setarray(request1, NUMR, 1, 0, 0, 0, 0); // Request resources
    pr(tid, "REQ", NUMR, request1);
    reman_request(request1);
    sleep(4);

    setarray(request2, NUMR, 0, 0, 0, 1, 1); // Request additional resources
    pr(tid, "REQ", NUMR, request2);
    reman_request(request2);

    reman_release(request1);
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
    i = 0;
    tids[i] = i;
    pthread_create(&(threadArray[i]), NULL, (void *)threadfunc1, (void *)(void *)&tids[i]);

    i = 1;
    tids[i] = i;
    pthread_create(&(threadArray[i]), NULL, (void *)threadfunc2, (void *)(void *)&tids[i]);

    i = 2;
    tids[i] = i;
    pthread_create(&(threadArray[i]), NULL, (void *)threadfunc3, (void *)(void *)&tids[i]);

    count = 0;
    while (count < 20)
    {
        sleep(1);
        reman_print("The current state");
        ret = reman_detect();
        if (ret > 0)
        {
            printf("deadlock detected, count=%d\n", ret);
            reman_print("state after deadlock");
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
