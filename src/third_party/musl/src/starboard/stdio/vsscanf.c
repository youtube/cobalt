#include <stdio.h>
#include "starboard/string.h"

int vsscanf(const char *restrict s, const char *restrict fmt, va_list ap) {
  return SbStringScan(s, fmt, ap);
}
