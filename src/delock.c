/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * Description: delegation lock with oversubscription support
 * Author: Huawei Dresden Research Center
 * Created: 2022-06-11
 */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <vsync/atomic.h>
#include <vsync/common/cache.h>

#include "delock.h"

/* Context state
 *
 * Context is initialized in LOCKED state (spin field).
 * When thread wants to delegate, it goes into _delock_delegate and once state
 * is copied, it sets the spin field to DELEGATE. Meaning that another thread
 * can perform the CS on the thread's behalf.
 *
 */
#define UNLOCKED 0
#define LOCKED   1
#define DELEGATE 2

/* Status values
 * When going into _delock_delegate, the thread sets its status to WAITING
 */
#define WAITING 1
#define ABORTED 0
#define DONE    2

#ifndef DELOCK_MAX_NESTING
    #define DELOCK_MAX_NESTING 3
#elif DELOCK_MAX_NESTING == 0
    #warning "DELOCK_MAX_NESTING=0, delegation disabled"
#elif DELOCK_MAX_NESTING < 0
    #error "DELOCK_MAX_NESTING should be >= 0"
#endif

#ifndef DELOCK_QUOTA
    #define DELOCK_QUOTA 8
#elif DELOCK_QUOTA < 1
    #error "DELOCK_QUOTA should be >= 0"
#endif

#ifndef VSYNC_CACHEALIGN
    #define VSYNC_CACHEALIGN __vsync_cachealign
#endif

#if defined(__aarch64__)
typedef struct {
    uintptr_t x19, x20;
    uintptr_t x21, x22;
    uintptr_t x23, x24;
    uintptr_t x25, x26;
    uintptr_t x27, x28;
    uintptr_t x29, x30;
    uintptr_t d8, d9;
    uintptr_t d10, d11;
    uintptr_t d12, d13;
    uintptr_t d14, d15;
    uintptr_t sp;
    uintptr_t tpidr;
    vatomic32_t status;
    uint32_t pad;
} VSYNC_CACHEALIGN state_t;

#elif defined(__x86_64__)
typedef struct {
    uint64_t rbp;
    uint64_t rbx;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rsp;
    uint64_t rax;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t fs;
    uint64_t gs;
    vatomic32_t status;
} VSYNC_CACHEALIGN state_t;

#else

    #error "architecture not supported"

#endif

extern int _delock_delegate(state_t *state, void *spin, vatomic32_t *status);
extern void _delock_swap(state_t *state, state_t *next, vatomic32_t *status,
                         uint32_t val);

/* *****************************************************************************
 * TLS-based stack of delegation contexts
 * ****************************************************************************/
static __thread struct {
    state_t state[DELOCK_MAX_NESTING];
    uint32_t idx;
} VSYNC_CACHEALIGN _sstack;

static inline void
_state_push(state_t **ptr)
{
    *ptr = NULL;
    if (_sstack.idx < DELOCK_MAX_NESTING)
        *ptr = &_sstack.state[_sstack.idx++];
}

static inline void
_state_pop(state_t **ptr)
{
    assert(_sstack.idx > 0);
    _sstack.idx--;
    if (ptr)
        *ptr = NULL;
}

/* *****************************************************************************
 * Contexts
 * ****************************************************************************/
typedef struct delock_ctx {
    vatomicptr_t next;
    vatomic32_t spin;

    state_t *state;  //< thread register state
    state_t *waiter; //< NULL or register state of a waiter thread
    int quota;
    int node;
    struct delock_ctx *jump;
} VSYNC_CACHEALIGN delock_ctx_t;

static inline void
_delock_ctx_init(delock_ctx_t *ctx, int node)
{
    vatomicptr_write_rlx(&ctx->next, NULL);
    vatomic32_write_rlx(&ctx->spin, LOCKED);
    ctx->quota  = DELOCK_QUOTA;
    ctx->node   = node;
    ctx->waiter = NULL;
    ctx->state  = NULL;
    ctx->jump   = NULL;
}


/* *****************************************************************************
 * Shuffling and searching for delegations
 * ****************************************************************************/
static inline delock_ctx_t *
_delock_shuffle(delock_ctx_t *me, delock_ctx_t *next)
{
    delock_ctx_t *cprev;
    delock_ctx_t *cur;
    delock_ctx_t *cnext;
    int node;

    if (next == NULL)
        return NULL;

    node  = me->node;
    cprev = cur = next;

#ifdef DELOCK_SHUFFLE_JUMP
    if (me->jump)
        cprev = cur = me->jump;
#endif

    cnext   = (delock_ctx_t *)vatomicptr_read_acq(&cur->next);
    int max = 32;
    for (; max-- && cnext && cur->node / 24 != node / 24;
         cprev = cur, cur = cnext,
         cnext = (delock_ctx_t *)vatomicptr_read_acq(&cnext->next)) {}

    if (cprev == next || cur == next || cnext == NULL ||
        cur->node / 24 != node / 24)
        return next;

    vatomicptr_write_rel(&me->next, cur);
    vatomicptr_write_rel(&cprev->next, cnext);
    vatomicptr_write_rel(&cur->next, next);
    cur->jump = cprev;
    return cur;
}

static inline delock_ctx_t *
_delock_next_waiter(delock_ctx_t *ctx)
{
    delock_ctx_t *next = (delock_ctx_t *)vatomicptr_read_acq(&ctx->next);
#ifdef DELOCK_SHUFFLE
    next = _delock_shuffle(ctx, next);
#endif
    if (next == NULL || vatomic32_read_acq(&next->spin) != DELEGATE)
        return NULL;
#ifdef DELOCK_CACHECHECK
    if (next->node / 4 == ctx->node / 4)
        return NULL;
#endif
    if (ctx->quota-- <= 0)
        return NULL;

    //    next->prev = ctx;
    return next;
}

/* *****************************************************************************
 * MCS delegation level
 * ****************************************************************************/
static inline void
_delock_delegation_start(delock_t *m, delock_ctx_t *ctx)
{
    delock_ctx_t *next;
    delock_ctx_t *serv = (delock_ctx_t *)vatomicptr_read_rlx(&m->lock);
    // delock_ctx_t *serv = ctx->prev;

    /* save my state for delock_release */
    assert(ctx->state);
    serv->waiter = ctx->state;

    /* try to move tail back to server */
    vatomicptr_write_rlx(&serv->next, NULL);
    if ((next = (delock_ctx_t *)vatomicptr_read_acq(&ctx->next)) == NULL) {
        if (vatomicptr_cmpxchg_rel(&m->tail, ctx, serv) == ctx)
            return;
        next = (delock_ctx_t *)vatomicptr_await_neq_acq(&ctx->next, NULL);
    }

    /* splice myself from queue */
    vatomicptr_write_rlx(&serv->next, next);
}

static int
_delock_slowpath_mcs_acquire(delock_t *m, delock_ctx_t *ctx)
{
    _state_push(&ctx->state);
    delock_ctx_t *tail;
    vatomic32_t *status = &ctx->state->status;

    /* Similar to MCS lock, exchange the tail wiht our context */
    tail = (delock_ctx_t *)vatomicptr_xchg(&m->tail, ctx);

    if (tail == NULL)
        return 1;

    /* The lock is already owned, so we append our context node */
    vatomicptr_write_rel(&tail->next, ctx);

    /* Now we prepare delegation and wait */
    if (ctx->state == NULL) {
        /* If state is null, there is no space left for delegation, we need
         * to simply wait. This is currently a spin-wait, but we could
         * change that to perform yield or wait on a futex. */
        vatomic32_await_eq_acq(&ctx->spin, UNLOCKED);
    } else {
        /* There is still space for delegation, so try to delegate. */
        vatomic32_write_rlx(status, WAITING);
        if (_delock_delegate(ctx->state, &ctx->spin, status)) {
            /* If successful, another thread is now returning here and will
             * execute the caller's CS. */
            _delock_delegation_start(m, ctx);
            return 0;
        }
        /* Delegation failed, the actual thread executes its own CS. */
    }
    return 1;
}

static void
_delock_slowpath_mcs_release(delock_t *m, delock_ctx_t *ctx)
{
    /* The server thread calls this function when it is stopping to serve and
     * will finally enter its own CS. Hence, if the next waiter is expecting a
     * delegation, we have to abort its wait loop. */
    _state_pop(&ctx->state);
    delock_ctx_t *next;

    /* Similar to MCS lock, if there is no waiter, unlock and return */
    if ((next = (delock_ctx_t *)vatomicptr_read_acq(&ctx->next)) == NULL) {
        if (vatomicptr_cmpxchg_rel(&m->tail, ctx, NULL) == ctx)
            return;
        next = (delock_ctx_t *)vatomicptr_await_neq_acq(&ctx->next, NULL);
    }
    /* If there is a waiter, unleash waiter */
    if (vatomic32_xchg_rel(&next->spin, UNLOCKED) == DELEGATE) {
        assert(next->state);
        state_t *snext = next->state;
        /* If waiter expects delegation, we must abort the delegation */
        vatomic32_write_rel(&snext->status, ABORTED);
    }
}

/* *****************************************************************************
 * Slowpath acquire and release -- public interface
 * ****************************************************************************/
void
delock_delegation_acquire(delock_t *m, int node)
{
    delock_ctx_t *next;
    delock_ctx_t ctx;

    /* Initialize mcs context */
    _delock_ctx_init(&ctx, node);

    /* Acquire mcs lock or delegate */
    if (!_delock_slowpath_mcs_acquire(m, &ctx)) {
        /* Delegation completed, the server thread enters the CS here */
        return;
    }

    /* Thread now owns the lock. Waiters that had their CS delegated never
     * return from the function above. Take fast path lock and save ctx
     * pointer in case we become a server.
     */
    vatomicptr_await_eq_set_acq(&m->lock, DELOCK_UNLOCKED, &ctx);

    /* If the next waiter is ready for delegation, we can execute its CS */
    if (ctx.state && (next = _delock_next_waiter(&ctx)))
        _delock_swap(ctx.state, next->state, &ctx.state->status, ABORTED);

    /* Finally, we release the MCS lock and enter our own CS. */
    _delock_slowpath_mcs_release(m, &ctx);

    /* We are not a server anymore, so we remove our ctx address from the
     * fastpath lock. */
    vatomicptr_write_rlx(&m->lock, DELOCK_LOCKED);
}

void
delock_delegation_release(delock_ctx_t *serv)
{
    assert(serv);

    /* We are the server thread and we just executed a delegated CS. Now we
     * check if we will perform another delegated CS and if not we swap back to
     * our initial context. */
    delock_ctx_t *next = _delock_next_waiter(serv);
    if (next == NULL)
        next = serv;

    /* Inform waiter for which we executed the CS, that the CS is done. */
    _delock_swap(serv->waiter, next->state, &serv->waiter->status, DONE);
    _state_pop(NULL);
}
