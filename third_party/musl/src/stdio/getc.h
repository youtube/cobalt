#include "stdio_impl.h"

#if !defined(STARBOARD)
#include "pthread_impl.h"

#ifdef __GNUC__
__attribute__((__noinline__))
#endif

static int locking_getc(FILE *f)
{
	if (a_cas(&f->lock, 0, MAYBE_WAITERS-1)) __lockfile(f);
	int c = getc_unlocked(f);
	if (a_swap(&f->lock, 0) & MAYBE_WAITERS)
		__wake(&f->lock, 1, 1);
	return c;
}
#endif

static inline int do_getc(FILE *f)
{
#if defined(STARBOARD)
  if (f->use_flock) {
    pthread_mutex_lock(&(f->flock));
  }
  getc_unlocked(f);
  if (f->use_flock) {
    pthread_mutex_unlock(&(f->flock));
  }
#else
	int l = f->lock;
	if (l < 0 || l && (l & ~MAYBE_WAITERS) == __pthread_self()->tid)
		return getc_unlocked(f);
	return locking_getc(f);
#endif
}
