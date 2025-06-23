#ifndef STARBOARD_ATOMIC_H_
#define STARBOARD_ATOMIC_H_

#include "starboard/configuration.h"
#include "starboard/types.h"

extern "C" {
typedef int8_t SbAtomic8;
typedef int32_t SbAtomic32;
static SbAtomic32 SbAtomicNoBarrier_CompareAndSwap(volatile SbAtomic32* ptr,
                                                   SbAtomic32 old_value,
                                                   SbAtomic32 new_value);
static void SbAtomicMemoryBarrier();
}
#endif
