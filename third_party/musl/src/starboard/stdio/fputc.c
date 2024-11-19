#include <stdio.h>
#include "starboard/common/log.h"

int fputc(int c, FILE *f) {
  // There is no Starboard implementation that writes to a FILE object so only
  // writing to stdout or stderr are handled.
  SB_DCHECK((f == stdout) || (f == stderr));
  const char ch = c;
// hack
#if 0
  if (f == stdout) {
    SbLog(kSbLogPriorityInfo, ch);
  } else {
    SbLog(kSbLogPriorityError, ch);
  }
#endif
  return ch;
}
