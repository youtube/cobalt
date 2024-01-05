#include <time.h>

#if SB_API_VERSION < 16

#include "starboard/client_porting/eztime/eztime.h"

time_t time(time_t *t) {
  return EzTimeTGetNow(t);
}

#endif  // SB_API_VERSION < 16
