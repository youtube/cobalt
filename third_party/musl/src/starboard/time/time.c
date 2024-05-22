#if SB_API_VERSION < 16

#include <time.h>

#include "starboard/time.h"

time_t time(time_t *t) {
  int64_t posix_us = SbTimeToPosix(SbTimeGetNow());
  int64_t posix_s = posix_us >= 0 ? posix_us / 1000000
                                  : (posix_us - 1000000 + 1) / 1000000;
  time_t time_s = (time_t)posix_s;
  if (t) {
    *t = time_s;
  }
  return time_s;
}

#endif  // SB_API_VERSION < 16
