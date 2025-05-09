#ifndef LOCK_H
#define LOCK_H

hidden void __lock(volatile int *);
hidden void __unlock(volatile int *);

#if defined(STARBOARD)
/* TODO: Cobalt b/416297670 - Implement locking. */
#define LOCK(x)
#define UNLOCK(x)
#else
#define LOCK(x) __lock(x)
#define UNLOCK(x) __unlock(x)
#endif  // defined(STARBOARD)

#endif
