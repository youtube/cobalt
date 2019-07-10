#include "locale_impl.h"
#include "libc.h"
#include "starboard/common/log.h"

// Starboard does not provide a mechanism to create or delete a locale.
locale_t __newlocale(int mask, const char *name, locale_t loc) {
  return NULL;
}

weak_alias(__newlocale, newlocale);
