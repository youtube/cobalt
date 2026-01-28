#include "stdio_impl.h"

#undef stdout

static unsigned char buf[BUFSIZ+UNGET];
hidden FILE __stdout_FILE = {
	.buf = buf+UNGET,
	.buf_size = sizeof buf-UNGET,
	.fd = 1,
	.flags = F_PERM | F_NORD,
	.lbf = '\n',
	.write = __stdout_write,
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
FILE *const stdout = &__stdout_FILE;
FILE *volatile __stdout_used = &__stdout_FILE;
