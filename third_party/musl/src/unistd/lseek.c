#include <unistd.h>
#include "syscall.h"

off_t __lseek(int fd, off_t offset, int whence)
{
#if defined(STARBOARD)
	return lseek(fd, offset, whence);
#else
#ifdef SYS__llseek
	off_t result;
	return syscall(SYS__llseek, fd, offset>>32, offset, &result, whence) ? -1 : result;
#else
	return syscall(SYS_lseek, fd, offset, whence);
#endif
#endif
}

weak_alias(__lseek, lseek);
