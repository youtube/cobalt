
#include "starboard/common/log.h"

// TODO: b/413130400 - Cobalt: Remove this file and implement linker stubs

#define COBALT_LINKER_STUB_MSG "This is a stub needed for linking cobalt. You need to remove COBALT_LINKER_STUB and implement  " << SB_FUNCTION

#define COBALT_LINKER_STUB()                              \
  do {                                                    \
    static int count = 0;                                 \
    if (0 == count++) {                                   \
      SB_LOG(ERROR) << COBALT_LINKER_STUB_MSG;            \
    }                                                     \
  } while (0);                                            \
  SbSystemBreakIntoDebugger();