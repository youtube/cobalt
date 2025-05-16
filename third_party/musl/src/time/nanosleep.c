#include <time.h>
#if defined(USE_COBALT_CUSTOMIZATIONS)
#include "syscall.h"
#endif  // defined(USE_COBALT_CUSTOMIZATIONS)

int nanosleep(const struct timespec *req, struct timespec *rem)
{
#if defined(USE_COBALT_CUSTOMIZATIONS)
	return clock_nanosleep(CLOCK_REALTIME, 0, req, rem);
#else
	return __syscall_ret(-__clock_nanosleep(CLOCK_REALTIME, 0, req, rem));
#endif  // defined(USE_COBALT_CUSTOMIZATIONS)
}
