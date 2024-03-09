#if SB_API_VERSION < 16

#include <time.h>

#include "starboard/client_porting/eztime/eztime.h"

time_t mktime(struct tm * tm) {
  if (!tm) {
    return -1;
  }
  EzTimeExploded ezte;
  ezte.tm_sec = tm->tm_sec;
  ezte.tm_min = tm->tm_min;
  ezte.tm_hour = tm->tm_hour;
  ezte.tm_mday = tm->tm_mday;
  ezte.tm_mon = tm->tm_mon;
  ezte.tm_year = tm->tm_year;
  ezte.tm_wday = tm->tm_wday;
  ezte.tm_yday = tm->tm_yday;
  ezte.tm_isdst = tm->tm_isdst;

  EzTimeT ezt = EzTimeTImplodeLocal(&ezte);
  return (time_t)ezt;
}

#endif  // SB_API_VERSION < 16
