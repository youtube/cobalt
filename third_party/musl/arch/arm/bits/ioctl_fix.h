#if defined(STARBOARD)
#include "../../generic/bits/ioctl_fix.h"
#endif  // defined(STARBOARD)

#undef FIOQSIZE
#define FIOQSIZE 0x545e
