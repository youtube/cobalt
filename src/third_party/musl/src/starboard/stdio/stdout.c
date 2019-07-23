#include "stdio_impl.h"

// Force stdout to adopt the memory address of |f|, an arbitrary and unused
// FILE. We are able to get away with this hack since we do not care about or
// use the value of stdout, and simply want to ensure that when a FILE* is
// passed as a parameter it is equal to the memory address of stdout.
static const FILE f = { 0 };
FILE *const stdout = &f;
