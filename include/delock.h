/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 * SPDX-License-Identifier: MIT
 */
#ifndef _DELOCK_H
#define _DELOCK_H
/*******************************************************************************
 * @file delock.h
 * @brief Delegation lock with oversubscription support
 ******************************************************************************/

#include <stdint.h>
#include <stddef.h>
#include <vsync/atomic.h>

#include <assert.h>
#include <stdint.h>
#include <string.h>

/* configuration */
#define DELOCK_QUOTA       128
#define DELOCK_MAX_NESTING 8

//#define DELOCK_FASTPATH
//#define DELOCK_CACHECHECK
//#define DELOCK_SHUFFLE
//#define DELOCK_SHUFFLE_JUMP

/* constants */
#define DELOCK_UNLOCKED ((void *)0)
#define DELOCK_LOCKED   ((void *)1)

typedef struct {
    vatomicptr_t lock;
    vatomicptr_t tail;
} delock_t;

struct delock_ctx;

static inline void
delock_init(delock_t *m)
{
    vatomicptr_init(&m->tail, NULL);
    vatomicptr_init(&m->lock, DELOCK_UNLOCKED);
}

void delock_delegation_release(struct delock_ctx *prev);
void delock_delegation_acquire(delock_t *m, int node);


static inline int
delock_tryacquire(delock_t *m, int node)
{
    (void)node;
    void *old =
        vatomicptr_cmpxchg_acq(&m->lock, DELOCK_UNLOCKED, DELOCK_LOCKED);
    return old == DELOCK_UNLOCKED;
}


static inline void
delock_acquire(delock_t *m, int node)
{
#ifdef DELOCK_FASTPATH
    if (delock_tryacquire(m, node))
        return;
#endif
    delock_delegation_acquire(m, node);
}


static inline void
delock_release(delock_t *m)
{
    struct delock_ctx *ctx;
    ctx = (struct delock_ctx *)vatomicptr_read_rlx(&m->lock);
    if (ctx == DELOCK_LOCKED)
        vatomicptr_write_rel(&m->lock, DELOCK_UNLOCKED);
    else
        delock_delegation_release(ctx);
}

#endif /* _DELOCK_H */
