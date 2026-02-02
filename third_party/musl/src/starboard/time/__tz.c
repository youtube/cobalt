#include <time.h>

#include "time_impl.h"
#include "starboard/common/log.h"

// Cobalt's hermetic implementation of timezone gives us a
// platform agnostic method to query for the name of a timezone
// given a  struct tm.
const char *__tm_to_tzname(const struct tm *tm) {
  return tm->tm_isdst > 0 ? tzname[1] : tzname[0];
}
