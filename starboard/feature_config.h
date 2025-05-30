#include "build/build_config.h"

#if BUILDFLAG(IS_STARBOARD)
DECLARE_FEATURE(kFeatureFoo);
#endif
