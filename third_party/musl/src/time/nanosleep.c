#include <time.h>
#if defined(STARBOARD)
#include <errno.h>
#else
#include "syscall.h"
#endif  // defined(STARBOARD)

int nanosleep(const struct timespec *req, struct timespec *rem)
{
#if defined(STARBOARD)
  int result = clock_nanosleep(CLOCK_REALTIME, 0, req, rem);
  if (result > 0) {
    errno = result;
    return -1;
  }
  return 0;
#else
	return __syscall_ret(-__clock_nanosleep(CLOCK_REALTIME, 0, req, rem));
#endif  // defined(STARBOARD)
}
