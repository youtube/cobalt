#include "stdio_impl.h"

#undef stderr

static unsigned char buf[UNGET];
hidden FILE __stderr_FILE = {
	.buf = buf+UNGET,
	.buf_size = 0,
	.fd = 2,
	.flags = F_PERM | F_NORD,
	.lbf = -1,
#if defined(STARBOARD)
	.read = __stdio_read_stub,
	.write = __stdio_write_init,
	.seek = __stdio_seek_init,
	.close = __stdio_close,
	.lock = 0,  // Starboard apps are typically multithreaded, require locking.
#else
	.write = __stdio_write,
	.seek = __stdio_seek,
	.close = __stdio_close,
	.lock = -1,
#endif
};
FILE *const stderr = &__stderr_FILE;
FILE *volatile __stderr_used = &__stderr_FILE;
