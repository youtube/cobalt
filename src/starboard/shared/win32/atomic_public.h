// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WIN32_ATOMIC_PUBLIC_H_
#define STARBOARD_SHARED_WIN32_ATOMIC_PUBLIC_H_

#include "starboard/atomic.h"

#ifdef __cplusplus
extern "C" {
#endif

// Declarations for Windows Intrinsic Functions
// Defined here to avoid including Windows headers
// See https://msdn.microsoft.com/en-us/library/w5405h95.aspx

long _InterlockedCompareExchange(
  long volatile * Destination,
  long Exchange,
  long Comparand
);
#pragma intrinsic(_InterlockedCompareExchange)

__int64 _InterlockedCompareExchange64(
  __int64 volatile * Destination,
  __int64 Exchange,
  __int64 Comparand
);
#pragma intrinsic(_InterlockedCompareExchange64)

char _InterlockedCompareExchange8(
  char volatile * Destination,
  char Exchange,
  char Comparand
);
#pragma intrinsic(_InterlockedCompareExchange8)

long _InterlockedExchange(
  long volatile * Target,
  long Value
);
#pragma intrinsic(_InterlockedExchange)

__int64 _InterlockedExchange64(
  __int64 volatile * Target,
  __int64 Value
);
#pragma intrinsic(_InterlockedExchange64)

long _InterlockedExchangeAdd(
  long volatile * Addend,
  long Value
);
#pragma intrinsic(_InterlockedExchangeAdd)

__int64 _InterlockedExchangeAdd64(
  __int64 volatile * Addend,
  __int64 Value
);
#pragma intrinsic(_InterlockedExchangeAdd64)

void _ReadWriteBarrier(void);
#pragma intrinsic(_ReadWriteBarrier)

SB_C_FORCE_INLINE SbAtomic32
SbAtomicNoBarrier_CompareAndSwap(volatile SbAtomic32* ptr,
                                 SbAtomic32 old_value,
                                 SbAtomic32 new_value) {
  // Note this does a full memory barrier
  return _InterlockedCompareExchange(
      (volatile long*) ptr, (long) new_value, (long) old_value);
}

SB_C_FORCE_INLINE SbAtomic32
SbAtomicNoBarrier_Exchange(volatile SbAtomic32* ptr, SbAtomic32 new_value) {
  // Note this does a full memory barrier
  return _InterlockedExchange((volatile long*)ptr, (long)new_value);
}

SB_C_FORCE_INLINE SbAtomic32
SbAtomicNoBarrier_Increment(volatile SbAtomic32* ptr, SbAtomic32 increment) {
  return SbAtomicBarrier_Increment(ptr, increment);
}

SB_C_FORCE_INLINE SbAtomic32 SbAtomicBarrier_Increment(volatile SbAtomic32* ptr,
                                                       SbAtomic32 increment) {
  // Note InterlockedExchangeAdd does a full memory barrier
  return increment + _InterlockedExchangeAdd(
      (volatile long *)ptr, (long)increment);
}

SB_C_FORCE_INLINE SbAtomic32
SbAtomicAcquire_CompareAndSwap(volatile SbAtomic32* ptr,
                               SbAtomic32 old_value,
                               SbAtomic32 new_value) {
  // Note this does a full memory barrier
  return _InterlockedCompareExchange(
      (volatile long*) ptr, (long) new_value, (long) old_value);
}

SB_C_FORCE_INLINE SbAtomic32
SbAtomicRelease_CompareAndSwap(volatile SbAtomic32* ptr,
                               SbAtomic32 old_value,
                               SbAtomic32 new_value) {
  // Note this does a full memory barrier
  return _InterlockedCompareExchange(
      (volatile long*) ptr, (long) new_value, (long) old_value);
}

// NOTE: https://msdn.microsoft.com/en-us/library/f20w0x5e.aspx
// states _ReadWriteBarrier() is deprecated and
// recommends "atomic_thread_fence", which is C++11 and violates
// Starboard's "C-only header" policy
SB_C_FORCE_INLINE void SbAtomicMemoryBarrier() {
  _ReadWriteBarrier();
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

#if SB_API_VERSION >= SB_EXPERIMENTAL_API_VERSION
SB_C_FORCE_INLINE SbAtomic8
SbAtomicRelease_CompareAndSwap8(volatile SbAtomic8* ptr,
                               SbAtomic8 old_value,
                               SbAtomic8 new_value) {
  // Note this does a full memory barrier
  return _InterlockedCompareExchange8((volatile char*)ptr, new_value, old_value);
}

SB_C_FORCE_INLINE void
SbAtomicNoBarrier_Store8(volatile SbAtomic8* ptr, SbAtomic8 value) {
  *ptr = value;
}

SB_C_FORCE_INLINE SbAtomic8
SbAtomicNoBarrier_Load8(volatile const SbAtomic8* ptr) {
  return *ptr;
}
#endif

// 64-bit atomic operations (only available on 64-bit processors).
#if SB_HAS(64_BIT_ATOMICS)
SB_C_FORCE_INLINE SbAtomic64
SbAtomicNoBarrier_CompareAndSwap64(volatile SbAtomic64* ptr,
                                   SbAtomic64 old_value,
                                   SbAtomic64 new_value) {
  return _InterlockedCompareExchange64(ptr, new_value, old_value);
}

SB_C_FORCE_INLINE SbAtomic64
SbAtomicNoBarrier_Exchange64(volatile SbAtomic64* ptr, SbAtomic64 new_value) {
  return _InterlockedExchange64(ptr, new_value);
}

SB_C_FORCE_INLINE SbAtomic64
SbAtomicNoBarrier_Increment64(volatile SbAtomic64* ptr, SbAtomic64 increment) {
  return increment + _InterlockedExchangeAdd64(ptr, increment);
}

SB_C_FORCE_INLINE SbAtomic64
SbAtomicBarrier_Increment64(volatile SbAtomic64* ptr, SbAtomic64 increment) {
  // Note this does a full memory barrier
  return increment + _InterlockedExchangeAdd64(ptr, increment);
}

SB_C_FORCE_INLINE SbAtomic64
SbAtomicAcquire_CompareAndSwap64(volatile SbAtomic64* ptr,
                                 SbAtomic64 old_value,
                                 SbAtomic64 new_value) {
  // Note this does a full memory barrier
  return _InterlockedCompareExchange64(ptr, new_value, old_value);
}

SB_C_FORCE_INLINE SbAtomic64
SbAtomicRelease_CompareAndSwap64(volatile SbAtomic64* ptr,
                                 SbAtomic64 old_value,
                                 SbAtomic64 new_value) {
  // Note this does a full memory barrier
  return _InterlockedCompareExchange64(ptr, new_value, old_value);
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

#endif  // STARBOARD_SHARED_WIN32_ATOMIC_PUBLIC_H_
