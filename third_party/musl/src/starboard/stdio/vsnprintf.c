#include <stdio.h>
#include "starboard/string.h"

#if SB_API_VERSION < 16

int vsnprintf(char *restrict s, size_t n, const char *restrict fmt, va_list ap) {
  return SbStringFormat(s, n, fmt, ap);
}

#endif
