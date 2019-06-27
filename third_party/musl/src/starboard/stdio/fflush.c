#include <stdio.h>
#include "starboard/common/log.h"

int fflush(FILE *f) {
  SB_DCHECK(f == stderr);

  // Note that SbLogFlush() only flushes stderr and is a void function, so
  // any errors won't be handled.
  SbLogFlush();
  return 0;
}
