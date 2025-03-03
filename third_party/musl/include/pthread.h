#ifndef _PTHREAD_H
#define _PTHREAD_H

//#include "build/build_config.h"

#if defined(STARBOARD)

//#include "third_party/musl/src/starboard/pthread/pthread.h"
#include <stdint.h>
#include <time.h>

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_PRIO_INHERIT 1
#define PTHREAD_PROCESS_SHARED 1
#define PTHREAD_SCOPE_SYSTEM 0
#define PTHREAD_MUTEX_ERRORCHECK 2

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
#define PTHREAD_MUTEX_INITIALIZER \
  {}
#else
#define PTHREAD_MUTEX_INITIALIZER \
  { 0 }
#endif

// Max size of the native mutex type.
#define MUSL_PTHREAD_MUTEX_MAX_SIZE 80

// An opaque handle to a native mutex type with reserved memory
// buffer aligned at void  pointer type.
typedef union pthread_mutex_t {
  // Reserved memory in which the implementation should map its
  // native mutex type.
  uint8_t mutex_buffer[MUSL_PTHREAD_MUTEX_MAX_SIZE];

  // Guarantees alignment of the type to a void pointer.
  void* ptr;
} pthread_mutex_t;

// Max size of the native mutex attribute type.
#define MUSL_PTHREAD_MUTEX_ATTR_MAX_SIZE 40

// An opaque handle to a native mutex attribute type with reserved memory
// buffer aligned at void  pointer type.
typedef union pthread_mutexattr_t {
  // Reserved memory in which the implementation should map its
  // native mutex attribute type.
  uint8_t mutex_buffer[MUSL_PTHREAD_MUTEX_ATTR_MAX_SIZE];

  // Guarantees alignment of the type to a void pointer.
  void* ptr;
} pthread_mutexattr_t;

typedef uintptr_t pthread_key_t;

#ifdef __cplusplus
#define PTHREAD_COND_INITIALIZER \
  {}
#else
#define PTHREAD_COND_INITIALIZER \
  { 0 }
#endif

// Max size of the native conditional variable type.
#define MUSL_PTHREAD_COND_MAX_SIZE 80

// An opaque handle to a native conditional type with reserved memory
// buffer aligned at void  pointer type.
typedef union pthread_cond_t {
  // Reserved memory in which the implementation should map its
  // native conditional type.
  uint8_t cond_buffer[MUSL_PTHREAD_COND_MAX_SIZE];

  // Guarantees alignment of the type to a void pointer.
  void* ptr;
} pthread_cond_t;

// Max size of the native conditional attribute type.
#define MUSL_PTHREAD_COND_ATTR_MAX_SIZE 40

// An opaque handle to a native conditional attribute type with reserved memory
// buffer aligned at void  pointer type.
typedef union pthread_condattr_t {
  // Reserved memory in which the implementation should map its
  // native conditional attribute type.
  uint8_t cond_buffer[MUSL_PTHREAD_COND_ATTR_MAX_SIZE];

  // Guarantees alignment of the type to a void pointer.
  void* ptr;
} pthread_condattr_t;

// Max size of the native conditional attribute type.
#define MUSL_PTHREAD_ATTR_MAX_SIZE 80

// An opaque handle to a native attribute type with reserved memory
// buffer aligned at void  pointer type.
typedef union pthread_attr_t {
  // Reserved memory in which the implementation should map its
  // native attribute type.
  uint8_t attr_buffer[MUSL_PTHREAD_ATTR_MAX_SIZE];

  // Guarantees alignment of the type to a void pointer.
  void* ptr;
} pthread_attr_t;

typedef int clockid_t;
typedef uintptr_t pthread_t;

// Max size of the pthread_once_t type.
#define MUSL_PTHREAD_ONCE_MAX_SIZE 64

// An opaque handle to a once control type with
// aligned at void  pointer type.
typedef union pthread_once_t {
  // Reserved memory in which the implementation should map its
  // native once control variable type.
  uint8_t once_buffer[MUSL_PTHREAD_ONCE_MAX_SIZE];

  // Guarantees alignment of the type to a void pointer.
  void* ptr;
} pthread_once_t;



// Max size of the pthread_rwlock_t type.
#define MUSL_PTHREAD_RWLOCK_MAX_SIZE 80 

// An opaque handle to pthread_rwlock_t type with
// aligned at void  pointer type.
typedef union pthread_rwlock_t {
  // Reserved memory in which the implementation should map its
  // native once control variable type.
  uint8_t once_buffer[MUSL_PTHREAD_RWLOCK_MAX_SIZE];

  // Guarantees alignment of the type to a void pointer.
  void* ptr;
} pthread_rwlock_t;

// Max size of the pthread_rwlockattr_t type.
#define MUSL_PTHREAD_RWLOCK_ATTR_MAX_SIZE 80 

// An opaque handle to pthread_rwlockattr_t type with
// aligned at void  pointer type.
typedef union pthread_rwlockattr_t {
  // Reserved memory in which the implementation should map its
  // native once control variable type.
  uint8_t once_buffer[MUSL_PTHREAD_RWLOCK_ATTR_MAX_SIZE];

  // Guarantees alignment of the type to a void pointer.
  void* ptr;
} pthread_rwlockattr_t;



#ifdef __cplusplus
#define PTHREAD_ONCE_INIT \
  {}
#else
#define PTHREAD_ONCE_INIT \
  { 0 }
#endif

int pthread_mutex_destroy(pthread_mutex_t* mutex);
int pthread_mutex_init(pthread_mutex_t* mutex,
                       const pthread_mutexattr_t* mutex_attr);
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_unlock(pthread_mutex_t* mutex);
int pthread_mutex_trylock(pthread_mutex_t* mutex);

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*));
int pthread_key_delete(pthread_key_t key);
void* pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void* value);

int pthread_cond_broadcast(pthread_cond_t* cond);
int pthread_cond_destroy(pthread_cond_t* cond);
int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr);
int pthread_cond_signal(pthread_cond_t* cond);
int pthread_cond_timedwait(pthread_cond_t* cond,
                           pthread_mutex_t* mutex,
                           const struct timespec* t);
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);

int pthread_condattr_destroy(pthread_condattr_t* attr);
int pthread_condattr_getclock(const pthread_condattr_t* attr,
                              clockid_t* clock_id);
int pthread_condattr_init(pthread_condattr_t* attr);
int pthread_condattr_setclock(pthread_condattr_t* attr, clockid_t clock_id);

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void));

int pthread_create(pthread_t* thread,
                   const pthread_attr_t* attr,
                   void* (*start_routine)(void*),
                   void* arg);
int pthread_join(pthread_t thread, void** value_ptr);
int pthread_detach(pthread_t thread);
pthread_t pthread_self();
int pthread_equal(pthread_t t1, pthread_t t2);

int pthread_setname_np(pthread_t thread, const char* name);
int pthread_getname_np(pthread_t thread, char* name, size_t len);

int pthread_attr_destroy(pthread_attr_t *attr);
int pthread_attr_init(pthread_attr_t *attr);

int pthread_attr_getstacksize(const pthread_attr_t* attr, size_t* stack_size);
int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stack_size);

int pthread_attr_getdetachstate(const pthread_attr_t* att, int* detach_state);
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detach_state);

// TODO: b/399696581 - Cobalt: Implement pthread API's
int pthread_getattr_np(pthread_t, pthread_attr_t *);
int pthread_attr_getstack(const pthread_attr_t *__restrict, void **__restrict, size_t *__restrict);
int pthread_mutexattr_init(pthread_mutexattr_t *);
int pthread_mutexattr_destroy(pthread_mutexattr_t *);
int pthread_mutexattr_settype(pthread_mutexattr_t *, int);
int pthread_atfork(void (*)(void), void (*)(void), void (*)(void));
int pthread_mutexattr_setprotocol(pthread_mutexattr_t *, int);
int pthread_getschedparam(pthread_t, int *__restrict, struct sched_param *__restrict);
int pthread_setschedparam(pthread_t, int, const struct sched_param *);


int pthread_rwlock_init(pthread_rwlock_t *__restrict, const pthread_rwlockattr_t *__restrict);
int pthread_rwlock_destroy(pthread_rwlock_t *);
int pthread_rwlock_rdlock(pthread_rwlock_t *);
int pthread_rwlock_tryrdlock(pthread_rwlock_t *);
int pthread_rwlock_timedrdlock(pthread_rwlock_t *__restrict, const struct timespec *__restrict);
int pthread_rwlock_wrlock(pthread_rwlock_t *);
int pthread_rwlock_trywrlock(pthread_rwlock_t *);
int pthread_rwlock_timedwrlock(pthread_rwlock_t *__restrict, const struct timespec *__restrict);
int pthread_rwlock_unlock(pthread_rwlock_t *);


int pthread_mutexattr_setpshared(pthread_mutexattr_t *a, int pshared);
int pthread_attr_setschedpolicy(pthread_attr_t *, int);

int pthread_attr_getscope(const pthread_attr_t *__restrict, int *__restrict);
int pthread_attr_setscope(pthread_attr_t *, int);

#ifdef __cplusplus
}  // extern "C"
#endif


#else

#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>

#define __NEED_time_t
#define __NEED_clockid_t
#define __NEED_struct_timespec
#define __NEED_sigset_t
#define __NEED_pthread_t
#define __NEED_pthread_attr_t
#define __NEED_pthread_mutexattr_t
#define __NEED_pthread_condattr_t
#define __NEED_pthread_rwlockattr_t
#define __NEED_pthread_barrierattr_t
#define __NEED_pthread_mutex_t
#define __NEED_pthread_cond_t
#define __NEED_pthread_rwlock_t
#define __NEED_pthread_barrier_t
#define __NEED_pthread_spinlock_t
#define __NEED_pthread_key_t
#define __NEED_pthread_once_t
#define __NEED_size_t

#include <bits/alltypes.h>

#include <sched.h>
#include <time.h>

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_DEFAULT 0
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_MUTEX_ERRORCHECK 2

#define PTHREAD_MUTEX_STALLED 0
#define PTHREAD_MUTEX_ROBUST 1

#define PTHREAD_PRIO_NONE 0
#define PTHREAD_PRIO_INHERIT 1
#define PTHREAD_PRIO_PROTECT 2

#define PTHREAD_INHERIT_SCHED 0
#define PTHREAD_EXPLICIT_SCHED 1

#define PTHREAD_SCOPE_SYSTEM 0
#define PTHREAD_SCOPE_PROCESS 1

#define PTHREAD_PROCESS_PRIVATE 0
#define PTHREAD_PROCESS_SHARED 1


#define PTHREAD_MUTEX_INITIALIZER {{{0}}}
#define PTHREAD_RWLOCK_INITIALIZER {{{0}}}
#define PTHREAD_COND_INITIALIZER {{{0}}}
#define PTHREAD_ONCE_INIT 0


#define PTHREAD_CANCEL_ENABLE 0
#define PTHREAD_CANCEL_DISABLE 1
#define PTHREAD_CANCEL_MASKED 2

#define PTHREAD_CANCEL_DEFERRED 0
#define PTHREAD_CANCEL_ASYNCHRONOUS 1

#define PTHREAD_CANCELED ((void *)-1)


#define PTHREAD_BARRIER_SERIAL_THREAD (-1)


#define PTHREAD_NULL ((pthread_t)0)


int pthread_create(pthread_t *__restrict, const pthread_attr_t *__restrict, void *(*)(void *), void *__restrict);
int pthread_detach(pthread_t);
_Noreturn void pthread_exit(void *);
int pthread_join(pthread_t, void **);

#ifdef __GNUC__
__attribute__((const))
#endif
pthread_t pthread_self(void);

int pthread_equal(pthread_t, pthread_t);
#ifndef __cplusplus
#define pthread_equal(x,y) ((x)==(y))
#endif

int pthread_setcancelstate(int, int *);
int pthread_setcanceltype(int, int *);
void pthread_testcancel(void);
int pthread_cancel(pthread_t);

int pthread_getschedparam(pthread_t, int *__restrict, struct sched_param *__restrict);
int pthread_setschedparam(pthread_t, int, const struct sched_param *);
int pthread_setschedprio(pthread_t, int);

int pthread_once(pthread_once_t *, void (*)(void));

int pthread_mutex_init(pthread_mutex_t *__restrict, const pthread_mutexattr_t *__restrict);
int pthread_mutex_lock(pthread_mutex_t *);
int pthread_mutex_unlock(pthread_mutex_t *);
int pthread_mutex_trylock(pthread_mutex_t *);
int pthread_mutex_timedlock(pthread_mutex_t *__restrict, const struct timespec *__restrict);
int pthread_mutex_destroy(pthread_mutex_t *);
int pthread_mutex_consistent(pthread_mutex_t *);

int pthread_mutex_getprioceiling(const pthread_mutex_t *__restrict, int *__restrict);
int pthread_mutex_setprioceiling(pthread_mutex_t *__restrict, int, int *__restrict);

int pthread_cond_init(pthread_cond_t *__restrict, const pthread_condattr_t *__restrict);
int pthread_cond_destroy(pthread_cond_t *);
int pthread_cond_wait(pthread_cond_t *__restrict, pthread_mutex_t *__restrict);
int pthread_cond_timedwait(pthread_cond_t *__restrict, pthread_mutex_t *__restrict, const struct timespec *__restrict);
int pthread_cond_broadcast(pthread_cond_t *);
int pthread_cond_signal(pthread_cond_t *);

int pthread_rwlock_init(pthread_rwlock_t *__restrict, const pthread_rwlockattr_t *__restrict);
int pthread_rwlock_destroy(pthread_rwlock_t *);
int pthread_rwlock_rdlock(pthread_rwlock_t *);
int pthread_rwlock_tryrdlock(pthread_rwlock_t *);
int pthread_rwlock_timedrdlock(pthread_rwlock_t *__restrict, const struct timespec *__restrict);
int pthread_rwlock_wrlock(pthread_rwlock_t *);
int pthread_rwlock_trywrlock(pthread_rwlock_t *);
int pthread_rwlock_timedwrlock(pthread_rwlock_t *__restrict, const struct timespec *__restrict);
int pthread_rwlock_unlock(pthread_rwlock_t *);

int pthread_spin_init(pthread_spinlock_t *, int);
int pthread_spin_destroy(pthread_spinlock_t *);
int pthread_spin_lock(pthread_spinlock_t *);
int pthread_spin_trylock(pthread_spinlock_t *);
int pthread_spin_unlock(pthread_spinlock_t *);

int pthread_barrier_init(pthread_barrier_t *__restrict, const pthread_barrierattr_t *__restrict, unsigned);
int pthread_barrier_destroy(pthread_barrier_t *);
int pthread_barrier_wait(pthread_barrier_t *);

int pthread_key_create(pthread_key_t *, void (*)(void *));
int pthread_key_delete(pthread_key_t);
void *pthread_getspecific(pthread_key_t);
int pthread_setspecific(pthread_key_t, const void *);

int pthread_attr_init(pthread_attr_t *);
int pthread_attr_destroy(pthread_attr_t *);

int pthread_attr_getguardsize(const pthread_attr_t *__restrict, size_t *__restrict);
int pthread_attr_setguardsize(pthread_attr_t *, size_t);
int pthread_attr_getstacksize(const pthread_attr_t *__restrict, size_t *__restrict);
int pthread_attr_setstacksize(pthread_attr_t *, size_t);
int pthread_attr_getdetachstate(const pthread_attr_t *, int *);
int pthread_attr_setdetachstate(pthread_attr_t *, int);
int pthread_attr_getstack(const pthread_attr_t *__restrict, void **__restrict, size_t *__restrict);
int pthread_attr_setstack(pthread_attr_t *, void *, size_t);
int pthread_attr_getscope(const pthread_attr_t *__restrict, int *__restrict);
int pthread_attr_setscope(pthread_attr_t *, int);
int pthread_attr_getschedpolicy(const pthread_attr_t *__restrict, int *__restrict);
int pthread_attr_setschedpolicy(pthread_attr_t *, int);
int pthread_attr_getschedparam(const pthread_attr_t *__restrict, struct sched_param *__restrict);
int pthread_attr_setschedparam(pthread_attr_t *__restrict, const struct sched_param *__restrict);
int pthread_attr_getinheritsched(const pthread_attr_t *__restrict, int *__restrict);
int pthread_attr_setinheritsched(pthread_attr_t *, int);

int pthread_mutexattr_destroy(pthread_mutexattr_t *);
int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *__restrict, int *__restrict);
int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *__restrict, int *__restrict);
int pthread_mutexattr_getpshared(const pthread_mutexattr_t *__restrict, int *__restrict);
int pthread_mutexattr_getrobust(const pthread_mutexattr_t *__restrict, int *__restrict);
int pthread_mutexattr_gettype(const pthread_mutexattr_t *__restrict, int *__restrict);
int pthread_mutexattr_init(pthread_mutexattr_t *);
int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *, int);
int pthread_mutexattr_setprotocol(pthread_mutexattr_t *, int);
int pthread_mutexattr_setpshared(pthread_mutexattr_t *, int);
int pthread_mutexattr_setrobust(pthread_mutexattr_t *, int);
int pthread_mutexattr_settype(pthread_mutexattr_t *, int);

int pthread_condattr_init(pthread_condattr_t *);
int pthread_condattr_destroy(pthread_condattr_t *);
int pthread_condattr_setclock(pthread_condattr_t *, clockid_t);
int pthread_condattr_setpshared(pthread_condattr_t *, int);
int pthread_condattr_getclock(const pthread_condattr_t *__restrict, clockid_t *__restrict);
int pthread_condattr_getpshared(const pthread_condattr_t *__restrict, int *__restrict);

int pthread_rwlockattr_init(pthread_rwlockattr_t *);
int pthread_rwlockattr_destroy(pthread_rwlockattr_t *);
int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *, int);
int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *__restrict, int *__restrict);

int pthread_barrierattr_destroy(pthread_barrierattr_t *);
int pthread_barrierattr_getpshared(const pthread_barrierattr_t *__restrict, int *__restrict);
int pthread_barrierattr_init(pthread_barrierattr_t *);
int pthread_barrierattr_setpshared(pthread_barrierattr_t *, int);

int pthread_atfork(void (*)(void), void (*)(void), void (*)(void));

int pthread_getconcurrency(void);
int pthread_setconcurrency(int);

int pthread_getcpuclockid(pthread_t, clockid_t *);

struct __ptcb {
	void (*__f)(void *);
	void *__x;
	struct __ptcb *__next;
};

void _pthread_cleanup_push(struct __ptcb *, void (*)(void *), void *);
void _pthread_cleanup_pop(struct __ptcb *, int);

#define pthread_cleanup_push(f, x) do { struct __ptcb __cb; _pthread_cleanup_push(&__cb, f, x);
#define pthread_cleanup_pop(r) _pthread_cleanup_pop(&__cb, (r)); } while(0)

#ifdef _GNU_SOURCE
struct cpu_set_t;
int pthread_getaffinity_np(pthread_t, size_t, struct cpu_set_t *);
int pthread_setaffinity_np(pthread_t, size_t, const struct cpu_set_t *);
int pthread_getattr_np(pthread_t, pthread_attr_t *);
int pthread_setname_np(pthread_t, const char *);
int pthread_getname_np(pthread_t, char *, size_t);
int pthread_getattr_default_np(pthread_attr_t *);
int pthread_setattr_default_np(const pthread_attr_t *);
int pthread_tryjoin_np(pthread_t, void **);
int pthread_timedjoin_np(pthread_t, void **, const struct timespec *);
#endif

#if _REDIR_TIME64
__REDIR(pthread_mutex_timedlock, __pthread_mutex_timedlock_time64);
__REDIR(pthread_cond_timedwait, __pthread_cond_timedwait_time64);
__REDIR(pthread_rwlock_timedrdlock, __pthread_rwlock_timedrdlock_time64);
__REDIR(pthread_rwlock_timedwrlock, __pthread_rwlock_timedwrlock_time64);
#ifdef _GNU_SOURCE
__REDIR(pthread_timedjoin_np, __pthread_timedjoin_np_time64);
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif  // BUILDFLAG(IS_STARBOARD)
#endif
