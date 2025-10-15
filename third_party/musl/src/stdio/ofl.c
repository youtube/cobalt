#include "stdio_impl.h"
#include "lock.h"
#include "fork_impl.h"

static FILE *ofl_head;

#if defined(STARBOARD)
#include <pthread.h>
static pthread_mutex_t __ofl_mutex;
static pthread_once_t __ofl_once = PTHREAD_ONCE_INIT;

static void __ofl_init(void) {
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&__ofl_mutex, &attr);
	pthread_mutexattr_destroy(&attr);
}

static pthread_mutex_t* const ofl_lock = &__ofl_mutex;
#else
static volatile int ofl_lock[1];
volatile int *const __stdio_ofl_lockptr = ofl_lock;
#endif

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
