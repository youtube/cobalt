#include <errno.h>

#include "starboard/common/log.h"
#include "starboard/memory.h"
#include "starboard/once.h"
#include "starboard/thread.h"
#include "starboard/thread_types.h"

static SbThreadLocalKey g_errno_key = kSbThreadLocalKeyInvalid;
static SbOnceControl g_errno_once = SB_ONCE_INITIALIZER;

void initialize_errno_key(void) {
  g_errno_key = SbThreadCreateLocalKey(SbMemoryDeallocate);
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

  value = (int*)SbMemoryAllocate(sizeof(int));

  SB_DCHECK(value);
  SB_DCHECK(SbThreadSetLocalValue(g_errno_key, value));

  *value = 0;

  return value;
}
