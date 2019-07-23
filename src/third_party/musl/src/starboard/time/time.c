#include <time.h>
#include "starboard/client_porting/eztime/eztime.h"

time_t time(time_t *t) {
  return EzTimeTGetNow(t);
}
