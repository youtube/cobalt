#include "stdio_impl.h"
#include "aio_impl.h"

#include <unistd.h>

static int dummy(int fd)
{
	return fd;
}

weak_alias(dummy, __aio_close);

int __stdio_close(FILE *f)
{
#if defined(STARBOARD)
	return close( __aio_close(f->fd));
#else
	return __syscall(SYS_close, __aio_close(f->fd));
#endif
}
