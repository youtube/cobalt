#include <unistd.h>
#include <stdlib.h>

_Noreturn void _exit(int status)
{
#if defined(STARBOARD)
  exit(status);
#else
	_Exit(status);
#endif
}
