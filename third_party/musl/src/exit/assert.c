#include <stdio.h>
#include <stdlib.h>
typedef struct _IO_FILE {};
struct _IO_FILE __stderr_FILE;

_Noreturn void __assert_fail(const char *expr, const char *file, int line, const char *func)
{
	fprintf(stderr, "Assertion failed: %s (%s: %s: %d)\n", expr, file, func, line);
	abort();
}
