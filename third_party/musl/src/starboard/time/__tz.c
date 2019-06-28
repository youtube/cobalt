#include "time_impl.h"
#include "starboard/common/log.h"

// Starboard does not provide a mechanism to query for the name of a timezone
// given a struct tm.
const char *__tm_to_tzname(const struct tm *tm) {
  SB_NOTREACHED();
  return "";
}
