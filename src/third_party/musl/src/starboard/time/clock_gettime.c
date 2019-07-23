#include <time.h>
#include "starboard/common/log.h"
#include "starboard/time.h"

// Note that with the original musl implementation of clock_gettime(), errno is
// set when -1 is returned; specific errors are not set here.
int clock_gettime(clockid_t clk, struct timespec *ts) {
  // There are only Starboard implementations for monotonic and realtime clocks.
  // Starboard does also have SbTimeGetMonotonicThreadNow() for
  // CLOCK_PROCESS_CPUTIME_ID, but its definition is wrapped in
  // #if SB_HAS(TIME_THREAD_NOW) so it can't be used here.
  // CLOCK_PROCESS_CPUTIME_ID is potentially used by Cobalt though, so -1 will
  // be returned to indicate a failure instead of crashing with this DCHECK.
  SB_DCHECK((clk == CLOCK_MONOTONIC) || (clk == CLOCK_REALTIME) ||
            (clk == CLOCK_PROCESS_CPUTIME_ID));

  if (clk == CLOCK_MONOTONIC) {
    SbTimeMonotonic t = SbTimeGetMonotonicNow();
    if (t == 0) return -1;

    ts->tv_sec = t / kSbTimeSecond;
    ts->tv_nsec = (t % kSbTimeSecond) * kSbTimeNanosecondsPerMicrosecond;
    return 0;
  } else if (clk == CLOCK_REALTIME) {
    SbTime t = SbTimeGetNow();
    if (t == 0) return -1;

    ts->tv_sec = t / kSbTimeSecond;
    ts->tv_nsec = (t % kSbTimeSecond) * kSbTimeNanosecondsPerMicrosecond;
    return 0;
  } else {
    return -1;
  }
}
