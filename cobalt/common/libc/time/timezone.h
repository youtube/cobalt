#ifndef COBALT_COMMON_LIBC_TIME_TIMEZONE_H_
#define COBALT_COMMON_LIBC_TIME_TIMEZONE_H_

#include <time.h>
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace cobalt {
namespace common {
namespace libc {
namespace time {

// Sets the timezone fields on a `struct tm` object.
void SetTmTimezoneFields(struct tm* tm, const icu::TimeZone& zone, UDate date);

}  // namespace time
}  // namespace libc
}  // namespace common
}  // namespace cobalt

#endif  // COBALT_COMMON_LIBC_TIME_TIMEZONE_H_
