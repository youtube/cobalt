// This seems to be defined somewhere else, so we disable this definition for 
// starboard builds to avoid compile errors.
// TODO: (b/437179183) Investigate to see if we can reduce/simplify this code.
#if !defined(STARBOARD)
#define _GNU_SOURCE
#endif
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include "syscall.h"
// Without this declaration, Starboard builds are unable to find this function.
// TODO: (b/437179183) Investigate to see if we can reduce/simplify this code.
#if defined(STARBOARD)
long __syscall_ret(unsigned long);
#endif

int __futimesat(int dirfd, const char *pathname, const struct timeval times[2])
{
	struct timespec ts[2];
	if (times) {
		int i;
		for (i=0; i<2; i++) {
			if (times[i].tv_usec >= 1000000ULL)
				return __syscall_ret(-EINVAL);
			ts[i].tv_sec = times[i].tv_sec;
			ts[i].tv_nsec = times[i].tv_usec * 1000;
		}
	}
	return utimensat(dirfd, pathname, times ? ts : 0, 0);
}

// We disable this weak_alias function for Starboard builds as is not able
// to find the function.
// TODO: (b/437179183) Investigate to see if we can reduce/simplify this code.
#if !defined(STARBOARD)
weak_alias(__futimesat, futimesat);
#endif
