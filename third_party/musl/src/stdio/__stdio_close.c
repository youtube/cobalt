#include "stdio_impl.h"
#include "aio_impl.h"

static int dummy(int fd)
{
	return fd;
}

weak_alias(dummy, __aio_close);

int __stdio_close(FILE *f)
{
#ifdef STARBOARD
	f->lock = __destroy_file_lock(f);
#endif
	return syscall(SYS_close, __aio_close(f->fd));
}
