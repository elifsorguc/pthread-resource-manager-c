#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "reman.h"

#define NUMT 3
#define NUMR 5

void *thread_func(void *arg)
{
    int tid = *(int *)arg;
    reman_connect(tid);

    int claim[NUMR] = {1, 1, 0, 1, 0};
    reman_claim(claim);

    int req[NUMR] = {1, 0, 0, 1, 0};
    reman_request(req);
    sleep(2);

    int release[NUMR] = {1, 0, 0, 1, 0};
    reman_release(release);

    reman_disconnect();
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: ./app <avoid_flag>\n");
        exit(1);
    }

    int avoid_flag = atoi(argv[1]);
    reman_init(NUMT, NUMR, avoid_flag);

    pthread_t threads[NUMT];
    int tids[NUMT];
    for (int i = 0; i < NUMT; i++)
    {
        tids[i] = i;
        pthread_create(&threads[i], NULL, thread_func, &tids[i]);
    }

    for (int i = 0; i < NUMT; i++)
    {
        pthread_join(threads[i], NULL);
    }

    reman_print("Final state");
    return 0;
}