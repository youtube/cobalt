#include <errno.h>
#include <stdlib.h>

#include "starboard/common/log.h"
#include "starboard/thread.h"
#include "../pthread/pthread.h"

static pthread_key_t g_errno_key = 0;
static pthread_once_t g_errno_once = PTHREAD_ONCE_INIT;

void initialize_errno_key(void) {
  pthread_key_create(&g_errno_key , free);
  SB_DCHECK(g_errno_key);
}

// __errno_location() provides every thread with its own copy of |errno|.
//
// The first time __errno_location() is invoked it will initialize a global key.
// This key will then by used by every thread to set, and get, their instance of
// errno from thread-local storage.

int *__errno_location(void) {
  int result = pthread_once(&g_errno_once, &initialize_errno_key);
  SB_DCHECK(result == 0);

  int* value = (int*)pthread_getspecific(g_errno_key);

  if (value) {
    return value;
  }

  value = (int*)malloc(sizeof(int));

  SB_DCHECK(value);
  result = pthread_setspecific(g_errno_key, value);
  SB_DCHECK(result == 0);

  *value = 0;

  return value;
}

weak_alias(__errno_location, ___errno_location);
<<<<<<< HEAD
=======

#endif // SB_API_VERSION < 16
>>>>>>> 9cd5e1d5efb (Deprecate SbThreadLocalKey (#3096))
