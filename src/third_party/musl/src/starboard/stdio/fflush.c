#include <stdio.h>
#include "starboard/common/log.h"

int fflush(FILE *f) {
  // Note that SbLogFlush() only flushes stderr and is a void function, so
  // any errors won't be handled.
  SB_DCHECK((f == stdout) || (f == stderr) || (f == NULL));
  SbLogFlush();
  return 0;
}
