#ifndef LOCK_H
#define LOCK_H

#if defined(STARBOARD)
#include <pthread.h>
#define LOCK(x) pthread_mutex_lock(x)
#define UNLOCK(x) pthread_mutex_unlock(x)
#else
hidden void __lock(volatile int *);
hidden void __unlock(volatile int *);
#define LOCK(x) __lock(x)
#define UNLOCK(x) __unlock(x)
#endif

#endif
