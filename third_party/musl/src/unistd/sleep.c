#if defined(STARBOARD)
#include <errno.h>
#endif
#include <unistd.h>
#include <time.h>

unsigned sleep(unsigned seconds)
{
	struct timespec tv = { .tv_sec = seconds, .tv_nsec = 0 };
#if defined(STARBOARD)
  int saved_errno = errno;
  if (nanosleep(&tv, &tv)) {
    errno = saved_errno;
    return tv.tv_sec;
  }
#else
	if (nanosleep(&tv, &tv))
		return tv.tv_sec;
	return 0;
#endif
}
