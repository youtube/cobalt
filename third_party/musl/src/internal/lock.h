#ifndef LOCK_H
#define LOCK_H

#if defined(STARBOARD)
#include "pthread_impl.h"

hidden void __lock(StarboardPthreadCondMutex *);
hidden void __unlock(StarboardPthreadCondMutex *);

// Add a compile time static assert for the type of |x| to ensure that it's a
// pointer to StarboardPthreadCondMutex. This ensures that the address pointed 
// to has enough storage for the pthread mutex and conditional variable used
// to emulate the futex.
#define LOCK(x) \
    ( \
        (void)sizeof(struct { \
            _Static_assert(__builtin_types_compatible_p(__typeof__(x), StarboardPthreadCondMutex *), \
                           "LOCK() argument must be of type StarboardPthreadCondMutex *"); \
            char __type_check; \
        }), \
        __lock(x) \
    )

#define UNLOCK(x) \
    ( \
        (void)sizeof(struct { \
            _Static_assert(__builtin_types_compatible_p(__typeof__(x), StarboardPthreadCondMutex *), \
                           "UNLOCK() argument must be of type StarboardPthreadCondMutex *"); \
            char __type_check; \
        }), \
        __unlock(x) \
    )

#else
hidden void __lock(volatile int *);
hidden void __unlock(volatile int *);
#define LOCK(x) __lock(x)
#define UNLOCK(x) __unlock(x)
#endif

#endif
