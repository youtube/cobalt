#include <sys/time.h>
#include "fcntl.h"
#include "syscall.h"
// Without this declaration, Starboard builds are unable to find this function.
// TODO: (b/437179183) Investigate to see if we can reduce/simplify this code.
#if defined(STARBOARD)
int __futimesat(int, const char *, const struct timeval [2]);
#endif

int utimes(const char *path, const struct timeval times[2])
{
	return __futimesat(AT_FDCWD, path, times);
}
