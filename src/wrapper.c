#ifndef LOCK_WRAPPER_H
#define LOCK_WRAPPER_H

#include <stdbool.h>
#include <sched.h>
#include <tilt.h>

#if defined(CONFIG_SPINLOCK)

    #include <vsync/spinlock.h>
    #define WLOCK_T                  spinlock_t
    #define WLOCK_INIT(_lock_ptr)    spinlock_init((_lock_ptr))
    #define WLOCK_ACQUIRE(_lock_ptr) spinlock_acquire((_lock_ptr))
    #define WLOCK_RELEASE(_lock_ptr) spinlock_release((_lock_ptr))

#elif defined(CONFIG_MUTEX)

    #include <vsync/mutex_musl.h>
    #define WLOCK_T                  mutex_t
    #define WLOCK_INIT(_lock_ptr)    mutex_init((_lock_ptr))
    #define WLOCK_ACQUIRE(_lock_ptr) mutex_lock((_lock_ptr))
    #define WLOCK_RELEASE(_lock_ptr) mutex_unlock((_lock_ptr))

#else
    #include "delock.h"

    #define WLOCK_T                  delock_t
    #define WLOCK_INIT(_lock_ptr)    delock_init((_lock_ptr))
    #define WLOCK_ACQUIRE(_lock_ptr) delock_acquire((_lock_ptr), sched_getcpu())
    #define WLOCK_TRYACQUIRE(_lock_ptr)                                        \
        delock_tryacquire((_lock_ptr), sched_getcpu())
    #define WLOCK_RELEASE(_lock_ptr) delock_release((_lock_ptr))

#endif

struct tilt_mutex {
    WLOCK_T lock;
};

void
tilt_mutex_unlock(struct tilt_mutex *m)
{
    WLOCK_RELEASE(&m->lock);
}

void
tilt_mutex_lock(struct tilt_mutex *m)
{
    WLOCK_ACQUIRE(&m->lock);
}

bool
tilt_mutex_trylock(struct tilt_mutex *m)
{
    return WLOCK_TRYACQUIRE(&m->lock) != 0;
}

void
tilt_mutex_init(struct tilt_mutex *m)
{
    WLOCK_INIT(&m->lock);
}

void
tilt_mutex_destroy(struct tilt_mutex *m)
{
    (void)m;
}


#endif /* LOCK_WRAPPER_H */
