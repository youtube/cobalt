#include <locale.h>

#include "starboard/common/log.h"

// Starboard does not provide a mechanism to retrieve information about number
// formatting with the current locale.
struct lconv *localeconv(void) {
  SB_NOTREACHED();
  return NULL;
}
