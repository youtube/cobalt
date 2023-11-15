#include <stdio.h>
#include "starboard/string.h"

int vsnprintf(char *restrict s, size_t n, const char *restrict fmt, va_list ap) {
#if SB_API_VERSION < 16
  return SbStringFormat(s, n, fmt, ap);
#else
  return vsnprintf(s, n, fmt, ap);
#endif
}
