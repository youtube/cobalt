#include "stdio_impl.h"

#undef stdout

static unsigned char buf[BUFSIZ+UNGET];
hidden FILE __stdout_FILE = {
	.buf = buf+UNGET,
	.buf_size = sizeof buf-UNGET,
	.fd = 1,
	.flags = F_PERM | F_NORD,
	.lbf = '\n',
#if defined(STARBOARD)
	.read = __stdio_read_stub,
	.write = __stdio_write_init,
	.seek = __stdio_seek_init,
	.close = __stdio_close,
	.lock = 0,  // Starboard apps are typically multithreaded, require locking.
#else
	.write = __stdout_write,
	.seek = __stdio_seek,
	.close = __stdio_close,
	.lock = -1,
#endif
};
FILE *const stdout = &__stdout_FILE;
FILE *volatile __stdout_used = &__stdout_FILE;
