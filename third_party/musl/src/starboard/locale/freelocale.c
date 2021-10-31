#include "locale_impl.h"
#include "libc.h"
#include "starboard/common/log.h"

// Starboard does not provide a mechanism to create or delete a locale.
void freelocale(locale_t l) {
  return;
}

weak_alias(freelocale, __freelocale);
