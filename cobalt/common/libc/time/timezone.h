#ifndef COBALT_COMMON_LIBC_TIME_TIMEZONE_H_
#define COBALT_COMMON_LIBC_TIME_TIMEZONE_H_

#include <time.h>

#include "third_party/icu/source/common/unicode/utypes.h"

struct tm;

namespace cobalt {
namespace common {
namespace libc {
namespace time {

// Sets the tm_gmtoff and tm_zone members of a struct tm based on the
// provided ICU timezone and date. This uses the same logic as tzset()
// to determine timezone abbreviations and offsets.
void SetTmTimezoneFields(struct tm* tm, const UChar* zone_id, UDate date);

}  // namespace time
}  // namespace libc
}  // namespace common
}  // namespace cobalt

#endif  // COBALT_COMMON_LIBC_TIME_TIMEZONE_H_