#define _GNU_SOURCE
#include <sys/mman.h>
#include "syscall.h"

int mincore (void *addr, size_t len, unsigned char *vec)
{
#if defined(STARBOARD)
	return mincore(addr, len, vec);
#else
	return syscall(SYS_mincore, addr, len, vec);
#endif
}
