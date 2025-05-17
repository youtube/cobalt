#ifndef LOCK_H
#define LOCK_H

#if defined(STARBOARD)
#include <pthread.h>

hidden void __lock(pthread_mutex_t *);
hidden void __unlock(pthread_mutex_t *);
#else
hidden void __lock(volatile int *);
hidden void __unlock(volatile int *);
#endif

#define LOCK(x) __lock(x)
#define UNLOCK(x) __unlock(x)

#endif
