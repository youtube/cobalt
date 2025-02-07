#include <wchar.h>
#include "starboard/string.h"

#if SB_API_VERSION < 16

int vswprintf(wchar_t *restrict s, size_t n, const wchar_t *restrict fmt, va_list ap) {
  return SbStringFormatWide(s, n, fmt, ap);
}

#endif
