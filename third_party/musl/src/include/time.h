#ifndef TIME_H
#define TIME_H

#include "../../include/time.h"

hidden int __clock_gettime(clockid_t, struct timespec *);
#if defined(USE_COBALT_CUSTOMIZATIONS)
hidden static inline int __clock_nanosleep(clockid_t clockid,
                             int flags,
                             const struct timespec* ts,
                             struct timespec* remain) {
  clock_nanosleep(clockid, flags, ts, remain);
}
#else
hidden int __clock_nanosleep(clockid_t, int, const struct timespec *, struct timespec *);
#endif  // defined(USE_COBALT_CUSTOMIZATIONS)

hidden char *__asctime_r(const struct tm *, char *);
hidden struct tm *__gmtime_r(const time_t *restrict, struct tm *restrict);
hidden struct tm *__localtime_r(const time_t *restrict, struct tm *restrict);

hidden size_t __strftime_l(char *restrict, size_t, const char *restrict, const struct tm *restrict, locale_t);

#endif
