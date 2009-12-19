/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * GPL HEADER START
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; If not, see
 * http://www.sun.com/software/products/lustre/docs/GPLv2.pdf
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *
 * GPL HEADER END
 */
/*
 * Copyright  2008 Sun Microsystems, Inc. All rights reserved
 * Use is subject to license terms.
 */
/*
 * This file is part of Lustre, http://www.lustre.org/
 * Lustre is a trademark of Sun Microsystems, Inc.
 *
 * libcfs/include/libcfs/user-lock.h
 *
 * Author: Nikita Danilov <nikita@clusterfs.com>
 */

#ifndef __LIBCFS_USER_LOCK_H__
#define __LIBCFS_USER_LOCK_H__

#ifndef __LIBCFS_LIBCFS_H__
#error Do not #include this file directly. #include <libcfs/libcfs.h> instead
#endif

/* Implementations of portable synchronization APIs for liblustre */

/*
 * liblustre is single-threaded, so most "synchronization" APIs are trivial.
 *
 * XXX Liang: There are several branches share lnet with b_hd_newconfig,
 * if we define lock APIs at here, there will be conflict with liblustre
 * in other branches.
 */

#ifndef __KERNEL__

/*
 * The userspace implementations of linux/spinlock.h vary; we just
 * include our own for all of them
 */
#define __LINUX_SPINLOCK_H

/*
 * Optional debugging (magic stamping and checking ownership) can be added.
 */

/*
 * spin_lock
 *
 * - spin_lock_init(x)
 * - spin_lock(x)
 * - spin_unlock(x)
 * - spin_trylock(x)
 *
 * - spin_lock_irqsave(x, f)
 * - spin_unlock_irqrestore(x, f)
 *
 * No-op implementation.
 */
struct spin_lock {int foo;};

typedef struct spin_lock spinlock_t;

#define SPIN_LOCK_UNLOCKED (spinlock_t) { }
#define LASSERT_SPIN_LOCKED(lock) do {(void)sizeof(lock);} while(0)
#define LINVRNT_SPIN_LOCKED(lock) do {(void)sizeof(lock);} while(0)
#define LASSERT_SEM_LOCKED(sem) do {(void)sizeof(sem);} while(0)

void spin_lock_init(spinlock_t *lock);
void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);
int spin_trylock(spinlock_t *lock);
void spin_lock_bh_init(spinlock_t *lock);
void spin_lock_bh(spinlock_t *lock);
void spin_unlock_bh(spinlock_t *lock);

static inline int spin_is_locked(spinlock_t *l) {return 1;}
static inline void spin_lock_irqsave(spinlock_t *l, unsigned long f){}
static inline void spin_unlock_irqrestore(spinlock_t *l, unsigned long f){}

/*
 * Semaphore
 *
 * - sema_init(x, v)
 * - __down(x)
 * - __up(x)
 */
typedef struct semaphore {
    int foo;
} mutex_t;

void sema_init(struct semaphore *s, int val);
void __down(struct semaphore *s);
void __up(struct semaphore *s);

/*
 * Mutex:
 *
 * - init_mutex(x)
 * - init_mutex_locked(x)
 * - mutex_up(x)
 * - mutex_down(x)
 */
#define DECLARE_MUTEX(name)     \
        struct semaphore name = { 1 }

#define mutex_up(s)                     __up(s)
#define up(s)                           mutex_up(s)
#define mutex_down(s)                   __down(s)
#define down(s)                         mutex_down(s)

#define init_MUTEX(x)                   sema_init(x, 1)
#define init_MUTEX_LOCKED(x)            sema_init(x, 0)
#define init_mutex(s)                   init_MUTEX(s)

/*
 * Completion:
 *
 * - init_completion(c)
 * - complete(c)
 * - wait_for_completion(c)
 */
struct completion {
        unsigned int done;
        cfs_waitq_t wait;
};
typedef int (*cfs_wait_handler_t) (int timeout);
void init_completion_module(cfs_wait_handler_t handler);
int  call_wait_handler(int timeout);
void init_completion(struct completion *c);
void complete(struct completion *c);
void wait_for_completion(struct completion *c);
int wait_for_completion_interruptible(struct completion *c);

#define COMPLETION_INITIALIZER(work) \
        { 0, __WAIT_QUEUE_HEAD_INITIALIZER((work).wait) }

#define DECLARE_COMPLETION(work) \
        struct completion work = COMPLETION_INITIALIZER(work)

#define INIT_COMPLETION(x)      ((x).done = 0)


/*
 * rw_semaphore:
 *
 * - init_rwsem(x)
 * - down_read(x)
 * - up_read(x)
 * - down_write(x)
 * - up_write(x)
 */
struct rw_semaphore {
        int foo;
};

void init_rwsem(struct rw_semaphore *s);
void down_read(struct rw_semaphore *s);
int down_read_trylock(struct rw_semaphore *s);
void down_write(struct rw_semaphore *s);
int down_write_trylock(struct rw_semaphore *s);
void up_read(struct rw_semaphore *s);
void up_write(struct rw_semaphore *s);
void fini_rwsem(struct rw_semaphore *s);

/*
 * read-write lock : Need to be investigated more!!
 * XXX nikita: for now, let rwlock_t to be identical to rw_semaphore
 *
 * - DECLARE_RWLOCK(l)
 * - rwlock_init(x)
 * - read_lock(x)
 * - read_unlock(x)
 * - write_lock(x)
 * - write_unlock(x)
 */
typedef struct rw_semaphore rwlock_t;
#define RW_LOCK_UNLOCKED        (rwlock_t) { }

#define rwlock_init(pl)         init_rwsem(pl)

#define read_lock(l)            down_read(l)
#define read_unlock(l)          up_read(l)
#define write_lock(l)           down_write(l)
#define write_unlock(l)         up_write(l)

static inline void
write_lock_irqsave(rwlock_t *l, unsigned long f) { write_lock(l); }
static inline void
write_unlock_irqrestore(rwlock_t *l, unsigned long f) { write_unlock(l); }

static inline void
read_lock_irqsave(rwlock_t *l, unsigned long f) { read_lock(l); }
static inline void
read_unlock_irqrestore(rwlock_t *l, unsigned long f) { read_unlock(l); }

/*
 * Atomic for user-space
 * Copied from liblustre
 */
typedef struct { volatile int counter; } atomic_t;

#define ATOMIC_INIT(i) { (i) }

#define atomic_read(a) ((a)->counter)
#define atomic_set(a,b) do {(a)->counter = b; } while (0)
#define atomic_dec_and_test(a) ((--((a)->counter)) == 0)
#define atomic_dec_and_lock(a,b) ((--((a)->counter)) == 0)
#define atomic_inc(a)  (((a)->counter)++)
#define atomic_dec(a)  do { (a)->counter--; } while (0)
#define atomic_add(b,a)  do {(a)->counter += b;} while (0)
#define atomic_add_return(n,a) ((a)->counter += n)
#define atomic_inc_return(a) atomic_add_return(1,a)
#define atomic_sub(b,a)  do {(a)->counter -= b;} while (0)
#define atomic_sub_return(n,a) ((a)->counter -= n)
#define atomic_dec_return(a)  atomic_sub_return(1,a)
#define atomic_add_unless(v, a, u) ((v)->counter != u ? (v)->counter += a : 0)
#define atomic_inc_not_zero(v) atomic_add_unless((v), 1, 0)


#ifdef HAVE_LIBPTHREAD
#include <pthread.h>

/*
 * Completion
 */

struct cfs_completion {
        int c_done;
        pthread_cond_t c_cond;
        pthread_mutex_t c_mut;
};

void cfs_init_completion(struct cfs_completion *c);
void cfs_fini_completion(struct cfs_completion *c);
void cfs_complete(struct cfs_completion *c);
void cfs_wait_for_completion(struct cfs_completion *c);

/*
 * atomic.h
 */

typedef struct { volatile int counter; } cfs_atomic_t;

int cfs_atomic_read(cfs_atomic_t *a);
void cfs_atomic_set(cfs_atomic_t *a, int b);
int cfs_atomic_dec_and_test(cfs_atomic_t *a);
void cfs_atomic_inc(cfs_atomic_t *a);
void cfs_atomic_dec(cfs_atomic_t *a);
void cfs_atomic_add(int b, cfs_atomic_t *a);
void cfs_atomic_sub(int b, cfs_atomic_t *a);

#endif /* HAVE_LIBPTHREAD */

/**************************************************************************
 *
 * Mutex interface.
 *
 **************************************************************************/

struct mutex {
        struct semaphore m_sem;
};

#define DEFINE_MUTEX(m) struct mutex m

static inline void mutex_init(struct mutex *mutex)
{
        init_mutex(&mutex->m_sem);
}

static inline void mutex_lock(struct mutex *mutex)
{
        mutex_down(&mutex->m_sem);
}

static inline void mutex_unlock(struct mutex *mutex)
{
        mutex_up(&mutex->m_sem);
}

/**
 * Try-lock this mutex.
 *
 *
 * \retval 0 try-lock succeeded (lock acquired).
 * \retval errno indicates lock contention.
 */
static inline int mutex_down_trylock(struct mutex *mutex)
{
        return 0;
}

/**
 * Try-lock this mutex.
 *
 * Note, return values are negation of what is expected from down_trylock() or
 * pthread_mutex_trylock().
 *
 * \retval 1 try-lock succeeded (lock acquired).
 * \retval 0 indicates lock contention.
 */
static inline int mutex_trylock(struct mutex *mutex)
{
        return !mutex_down_trylock(mutex);
}

static inline void mutex_destroy(struct mutex *lock)
{
}

/*
 * This is for use in assertions _only_, i.e., this function should always
 * return 1.
 *
 * \retval 1 mutex is locked.
 *
 * \retval 0 mutex is not locked. This should never happen.
 */
static inline int mutex_is_locked(struct mutex *lock)
{
        return 1;
}


/**************************************************************************
 *
 * Lockdep "implementation". Also see lustre_compat25.h
 *
 **************************************************************************/

struct lock_class_key {
        int foo;
};

static inline void lockdep_set_class(void *lock, struct lock_class_key *key)
{
}

static inline void lockdep_off(void)
{
}

static inline void lockdep_on(void)
{
}

/* This has to be a macro, so that can be undefined in kernels that do not
 * support lockdep. */
#define mutex_lock_nested(mutex, subclass) mutex_lock(mutex)
#define spin_lock_nested(lock, subclass) spin_lock(lock)
#define down_read_nested(lock, subclass) down_read(lock)
#define down_write_nested(lock, subclass) down_write(lock)


/* !__KERNEL__ */
#endif

/* __LIBCFS_USER_LOCK_H__ */
#endif
/*
 * Local variables:
 * c-indentation-style: "K&R"
 * c-basic-offset: 8
 * tab-width: 8
 * fill-column: 80
 * scroll-step: 1
 * End:
 */