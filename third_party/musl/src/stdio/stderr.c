#include "stdio_impl.h"

#undef stderr

static unsigned char buf[UNGET];
hidden FILE __stderr_FILE = {
	.buf = buf+UNGET,
	.buf_size = 0,
	.fd = 2,
	.flags = F_PERM | F_NORD,
	.lbf = -1,
	.write = __stdio_write,
	.seek = __stdio_seek,
	.close = __stdio_close,
#if defined(STARBOARD)
	.read = __stdio_read_stub,
	.lock = 0,  // Starboard apps are typicall multithreaded, require locking.
	.cond_mutex = { 0, PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP, PTHREAD_COND_INITIALIZER },
#else
	.lock = -1,
#endif
};
FILE *const stderr = &__stderr_FILE;
FILE *volatile __stderr_used = &__stderr_FILE;
