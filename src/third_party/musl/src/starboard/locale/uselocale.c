#include "libc.h"
#include "locale_impl.h"

// Starboard does not provide a mechanism to set the locale for the current
// thread.
locale_t __uselocale(locale_t new) {
  return &new;
}

weak_alias(__uselocale, uselocale);
