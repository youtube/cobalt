#include "stdio_impl.h"
#include "lock.h"
#include "fork_impl.h"

static FILE *ofl_head;

#if defined(STARBOARD)
static StarboardPthreadCondMutex __ofl_lock_cond_mutex;
StarboardPthreadCondMutex *const ofl_lock = &__ofl_lock_cond_mutex;

static pthread_once_t __ofl_once = PTHREAD_ONCE_INIT;
static void __ofl_init(void) {
	__cond_mutex_pair_init(ofl_lock);
}
#else
static volatile int ofl_lock[1];
volatile int *const __stdio_ofl_lockptr = ofl_lock;
#endif  // defined(STARBOARD)

FILE **__ofl_lock()
{
#if defined(STARBOARD)
	pthread_once(&__ofl_once, __ofl_init);
#endif
	LOCK(ofl_lock);
	return &ofl_head;
}

void __ofl_unlock()
{
	UNLOCK(ofl_lock);
}
