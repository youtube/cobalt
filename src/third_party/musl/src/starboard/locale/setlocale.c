#include <locale.h>

#include "locale_impl.h"
#include "starboard/common/log.h"
#include "starboard/system.h"

// Starboard provides a mechanism for querying for the system's locale name, but
// not for setting it.
char *setlocale(int cat, const char *name) {
  SB_DCHECK(!name);
  return SbSystemGetLocaleId();
}
