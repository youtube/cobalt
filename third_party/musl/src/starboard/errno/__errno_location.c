#if SB_API_VERSION < 16
#include <errno.h>
#include <stdlib.h>

#include "starboard/common/log.h"
#include "starboard/once.h"
#include "starboard/thread.h"

static SbThreadLocalKey g_errno_key = kSbThreadLocalKeyInvalid;
static SbOnceControl g_errno_once = SB_ONCE_INITIALIZER;

void initialize_errno_key(void) {
  g_errno_key = SbThreadCreateLocalKey(free);
  SB_DCHECK(g_errno_key != kSbThreadLocalKeyInvalid);
}

// __errno_location() provides every thread with its own copy of |errno|.
//
// The first time __errno_location() is invoked it will initialize a global key.
// This key will then by used by every thread to set, and get, their instance of
// errno from thread-local storage.

int *__errno_location(void) {
  SB_DCHECK(SbOnce(&g_errno_once, &initialize_errno_key));

  int* value = (int*)SbThreadGetLocalValue(g_errno_key);

  if (value) {
    return value;
  }

  value = (int*)malloc(sizeof(int));

  SB_DCHECK(value);
  SB_DCHECK(SbThreadSetLocalValue(g_errno_key, value));

  *value = 0;

  return value;
}

weak_alias(__errno_location, ___errno_location);
#endif // SB_API_VERSION < 16