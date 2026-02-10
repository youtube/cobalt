#include "stdio_impl.h"

#undef stdin

static unsigned char buf[BUFSIZ+UNGET];
hidden FILE __stdin_FILE = {
	.buf = buf+UNGET,
	.buf_size = sizeof buf-UNGET,
	.fd = 0,
	.flags = F_PERM | F_NOWR,
	.read = __stdio_read,
	.seek = __stdio_seek,
	.close = __stdio_close,
#if defined(STARBOARD)
	.write = __stdio_write_stub,
	.lock = 0,  // Starboard apps are typicall multithreaded, require locking.
	.cond_mutex = { 0, PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP, PTHREAD_COND_INITIALIZER },
#else
	.lock = -1,
#endif
};
FILE *const stdin = &__stdin_FILE;
FILE *volatile __stdin_used = &__stdin_FILE;
