#include "stdio_impl.h"
#include <fcntl.h>
#include <string.h>
#include <errno.h>

FILE *fopen(const char *restrict filename, const char *restrict mode)
{
	FILE *f;
	int fd;
	int flags;

	/* Check for valid initial mode character */
	if (!strchr("rwa", *mode)) {
		errno = EINVAL;
		return 0;
	}

	/* Compute the flags to pass to open() */
	flags = __fmodeflags(mode);

#if defined(STARBOARD)
	fd = open(filename, flags, 0666);
#else
	fd = sys_open(filename, flags, 0666);
#endif
	if (fd < 0) return 0;
	if (flags & O_CLOEXEC)
#if defined(STARBOARD)
		fcntl(fd, F_SETFD, FD_CLOEXEC);
#else
		__syscall(SYS_fcntl, fd, F_SETFD, FD_CLOEXEC);
#endif

	f = __fdopen(fd, mode);
	if (f) return f;

#if defined(STARBOARD)
	close(fd);
#else
	__syscall(SYS_close, fd);
#endif
	return 0;
}
