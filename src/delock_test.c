/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#define NTHREADS 10
#define ITERS    100
pthread_mutex_t mutex;
pthread_mutex_t mutex2;

#define SZ 1024 * 100
unsigned int shared[SZ];

void *
run(void *arg)
{
    (void)arg;
    for (int i = 0; i < ITERS; i++) {
        pthread_mutex_lock(&mutex);
        pthread_mutex_lock(&mutex2);

        for (volatile int j = 0; j < SZ; j++)
            shared[j]++;

        pthread_mutex_unlock(&mutex);
        pthread_mutex_unlock(&mutex2);
    }
    return NULL;
}

int
main()
{
    pthread_t t[NTHREADS];

    for (intptr_t i = 0; i < NTHREADS; i++)
        pthread_create(&t[i], 0, run, (void *)i);
    for (intptr_t i = 0; i < NTHREADS; i++)
        pthread_join(t[i], NULL);

    printf("final result: %d\n", shared[0]);
    assert(shared[0] == NTHREADS * ITERS && "unexpected sum");

    return 0;
}
