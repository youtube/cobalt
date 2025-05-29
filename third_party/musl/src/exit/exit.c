#include <stdint.h>
#include <stdlib.h>
#include "build/build_config.h"
#include "libc.h"

#if BUILDFLAG(IS_STARBOARD)
#include "starboard/system.h"
#endif  // BUILDFLAG(IS_STARBOARD)

#if !BUILDFLAG(IS_STARBOARD)
static void dummy()
{
}

/* atexit.c and __stdio_exit.c override these. the latter is linked
 * as a consequence of linking either __toread.c or __towrite.c. */
weak_alias(dummy, __funcs_on_exit);
weak_alias(dummy, __stdio_exit);
weak_alias(dummy, _fini);

extern weak hidden void (*const __fini_array_start)(void), (*const __fini_array_end)(void);

static void libc_exit_fini(void)
{
	uintptr_t a = (uintptr_t)&__fini_array_end;
	for (; a>(uintptr_t)&__fini_array_start; a-=sizeof(void(*)()))
		(*(void (**)())(a-sizeof(void(*)())))();
	_fini();
}

weak_alias(libc_exit_fini, __libc_exit_fini);
#endif // !BUILDFLAG(IS_STARBOARD)

_Noreturn void exit(int code)
{
#if BUILDFLAG(IS_STARBOARD)
	SbSystemBreakIntoDebugger();
#else   // !BUILDFLAG(IS_STARBOARD)
	__funcs_on_exit();
	__libc_exit_fini();
	__stdio_exit();
	_Exit(code);
#endif // BUILDFLAG(IS_STARBOARD)
}
