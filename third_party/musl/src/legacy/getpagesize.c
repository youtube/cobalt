#define _GNU_SOURCE
#include <unistd.h>
#include "libc.h"

int getpagesize(void)
{
#if defined(STARBOARD)
	return sysconf(_SC_PAGESIZE);
#else
	return PAGE_SIZE;
#endif
}
