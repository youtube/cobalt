#include "stdio_impl.h"
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "libc.h"

FILE *__fdopen(int fd, const char *mode)
{
	FILE *f;
	struct winsize wsz;

	/* Check for valid initial mode character */
	if (!strchr("rwa", *mode)) {
		errno = EINVAL;
		return 0;
	}

	/* Allocate FILE+buffer or fail */
	if (!(f=malloc(sizeof *f + UNGET + BUFSIZ))) return 0;

	/* Zero-fill only the struct, not the buffer */
	memset(f, 0, sizeof *f);

	/* Impose mode restrictions */
	if (!strchr(mode, '+')) f->flags = (*mode == 'r') ? F_NOWR : F_NORD;

	/* Apply close-on-exec flag */
#if defined(STARBOARD)
	if (strchr(mode, 'e')) fcntl(fd, F_SETFD, FD_CLOEXEC);
#else
	if (strchr(mode, 'e')) __syscall(SYS_fcntl, fd, F_SETFD, FD_CLOEXEC);
#endif

	/* Set append mode on fd if opened for append */
	if (*mode == 'a') {
#if defined(STARBOARD)
		int flags = fcntl(fd, F_GETFL);
#else
		int flags = __syscall(SYS_fcntl, fd, F_GETFL);
#endif
		if (!(flags & O_APPEND))
#if defined(STARBOARD)
			fcntl(fd, F_SETFL, flags | O_APPEND);
#else
			__syscall(SYS_fcntl, fd, F_SETFL, flags | O_APPEND);
#endif
		f->flags |= F_APP;
	}

	f->fd = fd;
	f->buf = (unsigned char *)f + sizeof *f + UNGET;
	f->buf_size = BUFSIZ;

	/* Activate line buffered mode for terminals */
	f->lbf = EOF;
#if defined(STARBOARD)
	if (!(f->flags & F_NOWR) && !ioctl, fd, TIOCGWINSZ, &wsz)
#else
	if (!(f->flags & F_NOWR) && !__syscall(SYS_ioctl, fd, TIOCGWINSZ, &wsz))
#endif
		f->lbf = '\n';

	/* Initialize op ptrs. No problem if some are unneeded. */
	f->read = __stdio_read;
	f->write = __stdio_write;
	f->seek = __stdio_seek;
	f->close = __stdio_close;

#if defined(STARBOARD)
  pthread_mutexattr_t mattr;
  int rv = pthread_mutexattr_init(&mattr);
  if (rv != 0) {
     errno = EIO;
     return 0;
  }
  rv = pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
  if (rv != 0) {
     errno = EIO;
     return 0;
  }
  rv = pthread_mutex_init(&(f->flock), &mattr);
  if (rv != 0) {
     errno = EIO;
     return 0;
  }
  pthread_mutexattr_destroy(&mattr);
  f->use_flock = 1;
#else
	if (!libc.threaded) f->lock = -1;
#endif

	/* Add new FILE to open file list */
	return __ofl_add(f);
}

weak_alias(__fdopen, fdopen);
