#include "stdio_impl.h"

#if defined(STARBOARD)
#include <pthread.h>
#else
#include "pthread_impl.h"
#endif

int __lockfile(FILE *f)
{
#if defined(STARBOARD)
  int ret = pthread_mutex_lock(&(f->flock));
  return ret == 0? 1:0;
#else
	int owner = f->lock, tid = __pthread_self()->tid;
	if ((owner & ~MAYBE_WAITERS) == tid)
		return 0;
	owner = a_cas(&f->lock, 0, tid);
	if (!owner) return 1;
	while ((owner = a_cas(&f->lock, 0, tid|MAYBE_WAITERS))) {
		if ((owner & MAYBE_WAITERS) ||
		    a_cas(&f->lock, owner, owner|MAYBE_WAITERS)==owner)
			__futexwait(&f->lock, owner|MAYBE_WAITERS, 1);
	}
	return 1;
#endif
}

void __unlockfile(FILE *f)
{
#if defined(STARBOARD)
  pthread_mutex_unlock(&(f->flock));
#else
	if (a_swap(&f->lock, 0) & MAYBE_WAITERS)
		__wake(&f->lock, 1, 1);
#endif
}
