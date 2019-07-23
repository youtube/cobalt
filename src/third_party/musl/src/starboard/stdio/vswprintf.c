#include <wchar.h>
#include "starboard/string.h"

int vswprintf(wchar_t *restrict s, size_t n, const wchar_t *restrict fmt, va_list ap) {
  return SbStringFormatWide(s, n, fmt, ap);
}
