#include "stdio_impl.h"
#include "lock.h"
#include "fork_impl.h"

static FILE *ofl_head;

#if defined(STARBOARD)
static StarboardPthreadCondMutex __ofl_lock_cond_mutex = {
    .lock = 0,  // Starboard apps are typicall multithreaded, require locking.
    .mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP,
    .cond = PTHREAD_COND_INITIALIZER
};
StarboardPthreadCondMutex *const ofl_lock = &__ofl_lock_cond_mutex;
#else
static volatile int ofl_lock[1];
volatile int *const __stdio_ofl_lockptr = ofl_lock;
#endif  // defined(STARBOARD)

FILE **__ofl_lock()
{
	LOCK(ofl_lock);
	return &ofl_head;
}

void __ofl_unlock()
{
	UNLOCK(ofl_lock);
}
