#ifndef STARBOARD_SHARED_GCC_ATOMIC_GCC_PUBLIC_H_
#define STARBOARD_SHARED_GCC_ATOMIC_GCC_PUBLIC_H_

#include "starboard/atomic.h"

#ifdef __cplusplus
extern "C" {
#endif

static SB_C_FORCE_INLINE SbAtomic32
SbAtomicNoBarrier_CompareAndSwap(volatile SbAtomic32* ptr,
                                 SbAtomic32 old_value,
                                 SbAtomic32 new_value) {
  __atomic_compare_exchange_n(ptr, &old_value, new_value, false,
                              __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  return old_value;
}

static SB_C_FORCE_INLINE void SbAtomicMemoryBarrier() {
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_GCC_ATOMIC_GCC_PUBLIC_H_
