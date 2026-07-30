#include <stdlib.h>
#include "starboard/system.h"

void abort() {
  SbSystemBreakIntoDebugger();
}
