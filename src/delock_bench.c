/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vsync/atomic.h>

#include <pthread.h>

#if 1
    #define LOCK_T          pthread_mutex_t
    #define LOCK_ACQUIRE(x) pthread_mutex_lock(x)
    #define LOCK_RELEASE(x) pthread_mutex_unlock(x)
#else
    #include "delock.h"
    #define LOCK_T          delock_t
    #define LOCK_ACQUIRE(x) delock_acquire(x, 0)
    #define LOCK_RELEASE(x) delock_release(x)
#endif

LOCK_T mutex;
#define SZ 100
struct entry {
    unsigned int count __attribute__((aligned(16)));
} shared[SZ];

vatomic32_t stop;

void *
run()
{
    while (!vatomic32_read_rlx(&stop)) {
        LOCK_ACQUIRE(&mutex);
        for (volatile int j = 0; j < SZ; j++)
            shared[j].count++;
        LOCK_RELEASE(&mutex);
    }
    return 0;
}

int
main(const int argc, const char *argv[])
{
    if (argc != 2) {
        printf("%s <nthreads>\n", argv[0]);
        return 1;
    }
    int nthread = atoi(argv[1]);
    printf("starting %d threads\n", nthread);

    pthread_t *thread = (pthread_t *)malloc(nthread * sizeof(pthread_t));

    for (int i = 0; i < nthread; i++)
        pthread_create(&thread[i], 0, run, 0);

    vatomic32_write_rlx(&stop, 1);

    for (int i = 0; i < nthread; i++)
        pthread_join(thread[i], 0);

    printf("%d %u\n", nthread, shared[0].count);
    return 0;
}
