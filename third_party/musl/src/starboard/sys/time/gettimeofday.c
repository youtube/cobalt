#if SB_API_VERSION < 16

#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#include "starboard/time.h"

int gettimeofday(struct timeval* tp, void* tzp) {
  if (tp == NULL) {
    return -1;
  }

  int64_t windows_time_micros = SbTimeGetNow();
  // Handle number of microseconds btw Jan 1, 1601 (UTC) and Jan 1, 1970 (UTC).
  int64_t posix_time_micros = windows_time_micros - 11644473600000000ULL;
  tp->tv_sec = (time_t)(posix_time_micros / 1000000);
  tp->tv_usec = (suseconds_t)(posix_time_micros % 1000000);
  return 0;
}

#endif  // SB_API_VERSION < 16
