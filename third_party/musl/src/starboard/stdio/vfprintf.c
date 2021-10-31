#include <stdio.h>
#include "starboard/common/log.h"

int vfprintf(FILE *restrict f, const char *restrict fmt, va_list ap) {
  // There is no Starboard implementation that writes to a FILE object so only
  // writing to stdout or stderr are handled.
  SB_DCHECK((f == stdout) || (f == stderr));
  SbLogFormat(fmt, ap);
  // SbLogFormat() is a void function but vfprintf() returns an int: the total
  // number of characters written. vsprintf() writes the formatted data from the
  // variable argument list to a string and returns the total number of
  // characters written. So vsprintf() is returned here to account for
  // SbLogFormat() being a void function.
  char buffer[16 * 1024];
  int char_count = vsprintf(buffer, fmt, ap);
  if (f == stdout) {
    SbLog(kSbLogPriorityInfo, buffer);
  } else if (f == stderr) {
    SbLog(kSbLogPriorityError, buffer);
  }
  return char_count;
}
