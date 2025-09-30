#include "stdio_impl.h"
#include "lock.h"
#include "fork_impl.h"

static FILE *ofl_head;

#if defined(STARBOARD)
#include <pthread.h>
static pthread_mutex_t __ofs_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t* const ofl_lock = &__ofs_mutex;
#else
static volatile int ofl_lock[1];
volatile int *const __stdio_ofl_lockptr = ofl_lock;
#endif

FILE **__ofl_lock()
{
	LOCK(ofl_lock);
	return &ofl_head;
}

void __ofl_unlock()
{
	UNLOCK(ofl_lock);
}
