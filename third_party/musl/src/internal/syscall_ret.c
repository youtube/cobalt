#include <errno.h>
// Since this function makes no use of anything in syscall.h,
// we disable in Starboard builds as build issues arise when enabled.
// TODO: (b/437179183) Investigate to see if we can reduce/simplify this code.
#if !defined(STARBOARD)
#include "syscall.h"
#endif

long __syscall_ret(unsigned long r)
{
	if (r > -4096UL) {
		errno = -r;
		return -1;
	}
	return r;
}
