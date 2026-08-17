#include "libc.h"

#if defined(STARBOARD)
struct __libc __libc = {
<<<<<<< HEAD
	.threaded = 1,
	.need_locks = 1,
	.can_do_threads = 1,
=======
	.can_do_threads = 1,
	.threaded = 1,
	.need_locks = 1,
>>>>>>> 1e6aea7c28 (posix: Fix race condition in musl fclose during global flush (#11866))
};
#else
struct __libc __libc;
#endif

size_t __hwcap;
char *__progname=0, *__progname_full=0;

weak_alias(__progname, program_invocation_short_name);
weak_alias(__progname_full, program_invocation_name);
