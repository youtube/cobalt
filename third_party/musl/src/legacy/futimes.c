// This seems to be defined somewhere else, so we disable this definition for 
// starboard builds to avoid compile errors.
// TODO: (b/437179183) Investigate to see if we can reduce/simplify this code.
#if !defined(STARBOARD)
#define _GNU_SOURCE
#endif 
#include <sys/stat.h>
#include <sys/time.h>

int futimes(int fd, const struct timeval tv[2])
{
	struct timespec times[2];
	if (!tv) return futimens(fd, 0);
	times[0].tv_sec  = tv[0].tv_sec;
	times[0].tv_nsec = tv[0].tv_usec * 1000;
	times[1].tv_sec  = tv[1].tv_sec;
	times[1].tv_nsec = tv[1].tv_usec * 1000;
	return futimens(fd, times);
}
