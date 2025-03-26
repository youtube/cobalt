#include <stdio.h>
#include "starboard/common/log.h"

int vfprintf(FILE *restrict f, const char *restrict fmt, va_list ap) {
  // There is no Starboard implementation that writes to a FILE object so only
  // writing to stdout or stderr are handled.
  SB_DCHECK((f == stdout) || (f == stderr));
  const int kBufferSize = 16 * 1024;
  char buffer[kBufferSize];
  int char_count = vsnprintf(buffer, kBufferSize, fmt, ap);
  if (f == stdout) {
    SbLog(kSbLogPriorityInfo, buffer);
  } else if (f == stderr) {
    SbLog(kSbLogPriorityError, buffer);
  }
  return char_count;
}
