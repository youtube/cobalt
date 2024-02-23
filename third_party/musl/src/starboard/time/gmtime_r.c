#if SB_API_VERSION < 16

#include <time.h>

#include "starboard/client_porting/eztime/eztime.h"

struct tm *gmtime_r(const time_t *restrict t, struct tm *restrict tm) {
  if (!t || !tm) {
    return NULL;
  }
  EzTimeT ezt = (EzTimeT)*t;
  EzTimeExploded ezte;
  if (EzTimeTExplodeUTC(&ezt, &ezte) == NULL) {
    return NULL;
  }
  tm->tm_sec = ezte.tm_sec;
  tm->tm_min = ezte.tm_min;
  tm->tm_hour = ezte.tm_hour;
  tm->tm_mday = ezte.tm_mday;
  tm->tm_mon = ezte.tm_mon;
  tm->tm_year = ezte.tm_year;
  tm->tm_wday = ezte.tm_wday;
  tm->tm_yday = ezte.tm_yday;
  tm->tm_isdst = ezte.tm_isdst;
  return tm;
}

#endif  // SB_API_VERSION < 16
