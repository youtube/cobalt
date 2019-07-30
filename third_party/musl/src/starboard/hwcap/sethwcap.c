#include "hwcap_impl.h"
#include "libc.h"
#include "starboard/cpu_features.h"

size_t __hwcap;

// Set __hwcap bitmask by Starboard CPU features API.
void init_musl_hwcap() {
  SbCPUFeatures features;
  if (SbCPUFeaturesGet(&features)) {
    __hwcap = features.hwcap;
  } else {
    __hwcap = 0;
  }
}
