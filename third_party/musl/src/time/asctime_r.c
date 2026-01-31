#include <time.h>
#include <stdio.h>
#include <langinfo.h>
#include "locale_impl.h"
#include "atomic.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_STARBOARD)
#include <errno.h>
#endif // BUILDFLAG(IS_STARBOARD)

char *__asctime_r(const struct tm *restrict tm, char *restrict buf)
{

// MUSL's nl_langinfo_l implementation hardcoded its values and only supported
// English. Since Cobalt's hermetic implementation fully implements a lot more 
// locales, we create these char arrays to hold the English values MUSL was using.
// This implementation is in correspondence to the POSIX specification:
// https://pubs.opengroup.org/onlinepubs/9799919799/functions/asctime.html
#if BUILDFLAG(IS_STARBOARD)
    static char wday_name[7][4] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    static char mon_name[12][4] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

	if (tm == NULL || buf == NULL ||
		tm->tm_wday < 0 || tm->tm_wday > 6 ||
		tm->tm_mon < 0 || tm->tm_mon > 11) {
			errno = EINVAL;
			return NULL;
	}
#endif // BUILDFLAG(IS_STARBOARD)
    if (snprintf(buf, 26, "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
#if BUILDFLAG(IS_STARBOARD)
		wday_name[tm->tm_wday],
		mon_name[tm->tm_mon],
#else // BUILDFLAG(IS_STARBOARD)
		__nl_langinfo_l(ABDAY_1+tm->tm_wday, C_LOCALE),
		__nl_langinfo_l(ABMON_1+tm->tm_mon, C_LOCALE),
#endif // BUILDFLAG(IS_STARBOARD)
		tm->tm_mday, tm->tm_hour,
		tm->tm_min, tm->tm_sec,
		1900 + tm->tm_year) >= 26)
	{
		/* ISO C requires us to use the above format string,
		 * even if it will not fit in the buffer. Thus asctime_r
		 * is _supposed_ to crash if the fields in tm are too large.
		 * We follow this behavior and crash "gracefully" to warn
		 * application developers that they may not be so lucky
		 * on other implementations (e.g. stack smashing..).
		 */
		a_crash();
	}
	return buf;
}

weak_alias(__asctime_r, asctime_r);
