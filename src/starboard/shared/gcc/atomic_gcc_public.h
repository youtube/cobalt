// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// These definitions were mostly extracted from Chromium's
// base/atomicops_internals_gcc.h.

#ifndef STARBOARD_SHARED_GCC_ATOMIC_GCC_PUBLIC_H_
#define STARBOARD_SHARED_GCC_ATOMIC_GCC_PUBLIC_H_

#include "starboard/atomic.h"

#ifdef __cplusplus
extern "C" {
#endif

SB_C_FORCE_INLINE SbAtomic32
SbAtomicNoBarrier_CompareAndSwap(volatile SbAtomic32* ptr,
                                 SbAtomic32 old_value,
                                 SbAtomic32 new_value) {
  SbAtomic32 prev_value;
  do {
    if (__sync_bool_compare_and_swap(ptr, old_value, new_value))
      return old_value;
    prev_value = *ptr;
  } while (prev_value == old_value);
  return prev_value;
}

SB_C_FORCE_INLINE SbAtomic32
SbAtomicNoBarrier_Exchange(volatile SbAtomic32* ptr, SbAtomic32 new_value) {
  SbAtomic32 old_value;
  do {
    old_value = *ptr;
  } while (!__sync_bool_compare_and_swap(ptr, old_value, new_value));
  return old_value;
}

SB_C_FORCE_INLINE SbAtomic32
SbAtomicNoBarrier_Increment(volatile SbAtomic32* ptr, SbAtomic32 increment) {
  return SbAtomicBarrier_Increment(ptr, increment);
}

SB_C_FORCE_INLINE SbAtomic32 SbAtomicBarrier_Increment(volatile SbAtomic32* ptr,
                                                       SbAtomic32 increment) {
  for (;;) {
    // Atomic exchange the old value with an incremented one.
    SbAtomic32 old_value = *ptr;
    SbAtomic32 new_value = old_value + increment;
    if (__sync_bool_compare_and_swap(ptr, old_value, new_value)) {
      // The exchange took place as expected.
      return new_value;
    }
    // Otherwise, *ptr changed mid-loop and we need to retry.
  }
}

SB_C_FORCE_INLINE SbAtomic32
SbAtomicAcquire_CompareAndSwap(volatile SbAtomic32* ptr,
                               SbAtomic32 old_value,
                               SbAtomic32 new_value) {
  // Since NoBarrier_CompareAndSwap uses __sync_bool_compare_and_swap, which
  // is a full memory barrier, none is needed here or below in Release.
  return SbAtomicNoBarrier_CompareAndSwap(ptr, old_value, new_value);
}

SB_C_FORCE_INLINE SbAtomic32
SbAtomicRelease_CompareAndSwap(volatile SbAtomic32* ptr,
                               SbAtomic32 old_value,
                               SbAtomic32 new_value) {
  return SbAtomicNoBarrier_CompareAndSwap(ptr, old_value, new_value);
}

SB_C_FORCE_INLINE void SbAtomicMemoryBarrier() {
  __sync_synchronize();
}

SB_C_FORCE_INLINE void SbAtomicNoBarrier_Store(volatile SbAtomic32* ptr,
                                               SbAtomic32 value) {
  *ptr = value;
}

SB_C_FORCE_INLINE void SbAtomicAcquire_Store(volatile SbAtomic32* ptr,
                                             SbAtomic32 value) {
  *ptr = value;
  SbAtomicMemoryBarrier();
}

SB_C_FORCE_INLINE void SbAtomicRelease_Store(volatile SbAtomic32* ptr,
                                             SbAtomic32 value) {
  SbAtomicMemoryBarrier();
  *ptr = value;
}

SB_C_FORCE_INLINE SbAtomic32
SbAtomicNoBarrier_Load(volatile const SbAtomic32* ptr) {
  return *ptr;
}

SB_C_FORCE_INLINE SbAtomic32
SbAtomicAcquire_Load(volatile const SbAtomic32* ptr) {
  SbAtomic32 value = *ptr;
  SbAtomicMemoryBarrier();
  return value;
}

SB_C_FORCE_INLINE SbAtomic32
SbAtomicRelease_Load(volatile const SbAtomic32* ptr) {
  SbAtomicMemoryBarrier();
  return *ptr;
}

// 64-bit atomic operations (only available on 64-bit processors).
#if SB_HAS(64_BIT_ATOMICS)
SB_C_FORCE_INLINE SbAtomic64
SbAtomicNoBarrier_CompareAndSwap64(volatile SbAtomic64* ptr,
                                   SbAtomic64 old_value,
                                   SbAtomic64 new_value) {
  SbAtomic64 prev_value;
  do {
    if (__sync_bool_compare_and_swap(ptr, old_value, new_value))
      return old_value;
    prev_value = *ptr;
  } while (prev_value == old_value);
  return prev_value;
}

SB_C_FORCE_INLINE SbAtomic64
SbAtomicNoBarrier_Exchange64(volatile SbAtomic64* ptr, SbAtomic64 new_value) {
  SbAtomic64 old_value;
  do {
    old_value = *ptr;
  } while (!__sync_bool_compare_and_swap(ptr, old_value, new_value));
  return old_value;
}

SB_C_FORCE_INLINE SbAtomic64
SbAtomicNoBarrier_Increment64(volatile SbAtomic64* ptr, SbAtomic64 increment) {
  return SbAtomicBarrier_Increment64(ptr, increment);
}

SB_C_FORCE_INLINE SbAtomic64
SbAtomicBarrier_Increment64(volatile SbAtomic64* ptr, SbAtomic64 increment) {
  for (;;) {
    // Atomic exchange the old value with an incremented one.
    SbAtomic64 old_value = *ptr;
    SbAtomic64 new_value = old_value + increment;
    if (__sync_bool_compare_and_swap(ptr, old_value, new_value)) {
      // The exchange took place as expected.
      return new_value;
    }
    // Otherwise, *ptr changed mid-loop and we need to retry.
  }
}

SB_C_FORCE_INLINE SbAtomic64
SbAtomicAcquire_CompareAndSwap64(volatile SbAtomic64* ptr,
                                 SbAtomic64 old_value,
                                 SbAtomic64 new_value) {
  return SbAtomicNoBarrier_CompareAndSwap64(ptr, old_value, new_value);
}

SB_C_FORCE_INLINE SbAtomic64
SbAtomicRelease_CompareAndSwap64(volatile SbAtomic64* ptr,
                                 SbAtomic64 old_value,
                                 SbAtomic64 new_value) {
  return SbAtomicNoBarrier_CompareAndSwap64(ptr, old_value, new_value);
}

SB_C_FORCE_INLINE void SbAtomicNoBarrier_Store64(volatile SbAtomic64* ptr,
                                                 SbAtomic64 value) {
  *ptr = value;
}

SB_C_FORCE_INLINE void SbAtomicAcquire_Store64(volatile SbAtomic64* ptr,
                                               SbAtomic64 value) {
  *ptr = value;
  SbAtomicMemoryBarrier();
}

SB_C_FORCE_INLINE void SbAtomicRelease_Store64(volatile SbAtomic64* ptr,
                                               SbAtomic64 value) {
  SbAtomicMemoryBarrier();
  *ptr = value;
}

SB_C_FORCE_INLINE SbAtomic64
SbAtomicNoBarrier_Load64(volatile const SbAtomic64* ptr) {
  return *ptr;
}

SB_C_FORCE_INLINE SbAtomic64
SbAtomicAcquire_Load64(volatile const SbAtomic64* ptr) {
  SbAtomic64 value = *ptr;
  SbAtomicMemoryBarrier();
  return value;
}

SB_C_FORCE_INLINE SbAtomic64
SbAtomicRelease_Load64(volatile const SbAtomic64* ptr) {
  SbAtomicMemoryBarrier();
  return *ptr;
}
#endif  // SB_HAS(64_BIT_ATOMICS)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_GCC_ATOMIC_GCC_PUBLIC_H_
