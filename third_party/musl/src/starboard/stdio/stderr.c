#include "stdio_impl.h"

// Force stderr to adopt the memory address of |f|, an arbitrary and unused
// FILE. We are able to get away with this hack since we do not care about or
// use the value of stderr, and simply want to ensure that when a FILE* is
// passed as a parameter it is equal to the memory address of stderr.
static const FILE f = { 0 };
FILE *const stderr = &f;
