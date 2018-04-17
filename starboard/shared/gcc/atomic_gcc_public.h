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

static SB_C_FORCE_INLINE SbAtomic32
SbAtomicNoBarrier_Exchange(volatile SbAtomic32* ptr, SbAtomic32 new_value) {
  return __atomic_exchange_n(ptr, new_value, __ATOMIC_RELAXED);
}

static SB_C_FORCE_INLINE SbAtomic32
SbAtomicNoBarrier_Increment(volatile SbAtomic32* ptr, SbAtomic32 increment) {
  return increment + __atomic_fetch_add(ptr, increment, __ATOMIC_RELAXED);
}

static SB_C_FORCE_INLINE SbAtomic32
SbAtomicBarrier_Increment(volatile SbAtomic32* ptr,
                          SbAtomic32 increment) {
  return increment + __atomic_fetch_add(ptr, increment, __ATOMIC_SEQ_CST);
}

static SB_C_FORCE_INLINE SbAtomic32
SbAtomicAcquire_CompareAndSwap(volatile SbAtomic32* ptr,
                               SbAtomic32 old_value,
                               SbAtomic32 new_value) {
  __atomic_compare_exchange_n(ptr, &old_value, new_value, false,
                              __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE);
  return old_value;
}

static SB_C_FORCE_INLINE SbAtomic32
SbAtomicRelease_CompareAndSwap(volatile SbAtomic32* ptr,
                               SbAtomic32 old_value,
                               SbAtomic32 new_value) {
  __atomic_compare_exchange_n(ptr, &old_value, new_value, false,
                              __ATOMIC_RELEASE, __ATOMIC_RELAXED);
  return old_value;
}

static SB_C_FORCE_INLINE void SbAtomicMemoryBarrier() {
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

static SB_C_FORCE_INLINE void SbAtomicNoBarrier_Store(volatile SbAtomic32* ptr,
                                                      SbAtomic32 value) {
  __atomic_store_n(ptr, value, __ATOMIC_RELAXED);
}

static SB_C_FORCE_INLINE void SbAtomicAcquire_Store(volatile SbAtomic32* ptr,
                                                    SbAtomic32 value) {
  __atomic_store_n(ptr, value, __ATOMIC_RELAXED);
  SbAtomicMemoryBarrier();
}

static SB_C_FORCE_INLINE void SbAtomicRelease_Store(volatile SbAtomic32* ptr,
                                                    SbAtomic32 value) {
  __atomic_store_n(ptr, value, __ATOMIC_RELEASE);
}

static SB_C_FORCE_INLINE SbAtomic32
SbAtomicNoBarrier_Load(volatile const SbAtomic32* ptr) {
  return __atomic_load_n(ptr, __ATOMIC_RELAXED);
}

static SB_C_FORCE_INLINE SbAtomic32
SbAtomicAcquire_Load(volatile const SbAtomic32* ptr) {
  return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
}

static SB_C_FORCE_INLINE SbAtomic32
SbAtomicRelease_Load(volatile const SbAtomic32* ptr) {
  SbAtomicMemoryBarrier();
  return __atomic_load_n(ptr, __ATOMIC_RELAXED);
}

// 8-bit atomic operations.
#if SB_API_VERSION >= SB_EXPERIMENTAL_API_VERSION
static SB_C_FORCE_INLINE SbAtomic8
SbAtomicRelease_CompareAndSwap8(volatile SbAtomic8* ptr,
                               SbAtomic8 old_value,
                               SbAtomic8 new_value) {
  __atomic_compare_exchange_n(ptr, &old_value, new_value, false,
                              __ATOMIC_RELEASE, __ATOMIC_RELAXED);
  return old_value;
}

static SB_C_FORCE_INLINE void
SbAtomicNoBarrier_Store8(volatile SbAtomic8* ptr, SbAtomic8 value) {
  __atomic_store_n(ptr, value, __ATOMIC_RELAXED);
}

static SB_C_FORCE_INLINE SbAtomic8
SbAtomicNoBarrier_Load8(volatile const SbAtomic8* ptr) {
  return __atomic_load_n(ptr, __ATOMIC_RELAXED);
}
#endif

// 64-bit atomic operations (only available on 64-bit processors).
#if SB_HAS(64_BIT_ATOMICS)
static SB_C_FORCE_INLINE SbAtomic64
SbAtomicNoBarrier_CompareAndSwap64(volatile SbAtomic64* ptr,
                                   SbAtomic64 old_value,
                                   SbAtomic64 new_value) {
  __atomic_compare_exchange_n(ptr, &old_value, new_value, false,
                              __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  return old_value;
}

static SB_C_FORCE_INLINE SbAtomic64
SbAtomicNoBarrier_Exchange64(volatile SbAtomic64* ptr, SbAtomic64 new_value) {
  return __atomic_exchange_n(ptr, new_value, __ATOMIC_RELAXED);
}

static SB_C_FORCE_INLINE SbAtomic64
SbAtomicNoBarrier_Increment64(volatile SbAtomic64* ptr, SbAtomic64 increment) {
  return increment + __atomic_fetch_add(ptr, increment, __ATOMIC_RELAXED);
}

static SB_C_FORCE_INLINE SbAtomic64
SbAtomicBarrier_Increment64(volatile SbAtomic64* ptr, SbAtomic64 increment) {
  return increment + __atomic_fetch_add(ptr, increment, __ATOMIC_SEQ_CST);
}

static SB_C_FORCE_INLINE SbAtomic64
SbAtomicAcquire_CompareAndSwap64(volatile SbAtomic64* ptr,
                                 SbAtomic64 old_value,
                                 SbAtomic64 new_value) {
  __atomic_compare_exchange_n(ptr, &old_value, new_value, false,
                              __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE);
  return old_value;
}

static SB_C_FORCE_INLINE SbAtomic64
SbAtomicRelease_CompareAndSwap64(volatile SbAtomic64* ptr,
                                 SbAtomic64 old_value,
                                 SbAtomic64 new_value) {
  __atomic_compare_exchange_n(ptr, &old_value, new_value, false,
                              __ATOMIC_RELEASE, __ATOMIC_RELAXED);
  return old_value;
}

static SB_C_FORCE_INLINE void
SbAtomicNoBarrier_Store64(volatile SbAtomic64* ptr,
                          SbAtomic64 value) {
  __atomic_store_n(ptr, value, __ATOMIC_RELAXED);
}

static SB_C_FORCE_INLINE void SbAtomicAcquire_Store64(volatile SbAtomic64* ptr,
                                                      SbAtomic64 value) {
  __atomic_store_n(ptr, value, __ATOMIC_RELAXED);
  SbAtomicMemoryBarrier();
}

static SB_C_FORCE_INLINE void SbAtomicRelease_Store64(volatile SbAtomic64* ptr,
                                                      SbAtomic64 value) {
  __atomic_store_n(ptr, value, __ATOMIC_RELEASE);
}

static SB_C_FORCE_INLINE SbAtomic64
SbAtomicNoBarrier_Load64(volatile const SbAtomic64* ptr) {
  return __atomic_load_n(ptr, __ATOMIC_RELAXED);
}

static SB_C_FORCE_INLINE SbAtomic64
SbAtomicAcquire_Load64(volatile const SbAtomic64* ptr) {
  return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
}

static SB_C_FORCE_INLINE SbAtomic64
SbAtomicRelease_Load64(volatile const SbAtomic64* ptr) {
  SbAtomicMemoryBarrier();
  return __atomic_load_n(ptr, __ATOMIC_RELAXED);
}
#endif  // SB_HAS(64_BIT_ATOMICS)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_GCC_ATOMIC_GCC_PUBLIC_H_
