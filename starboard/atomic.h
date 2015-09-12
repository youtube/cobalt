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

#ifndef STARBOARD_ATOMIC_H_
#define STARBOARD_ATOMIC_H_

#include "starboard/configuration.h"
#include "starboard/types.h"

typedef int32_t SbAtomic32;

// Atomically execute:
//      result = *ptr;
//      if (*ptr == old_value)
//        *ptr = new_value;
//      return result;
//
// I.e., replace "*ptr" with "new_value" if "*ptr" used to be "old_value".
// Always return the old value of "*ptr"
//
// This routine implies no memory barriers.
SbAtomic32 SbAtomicNoBarrier_CompareAndSwap(
  volatile SbAtomic32 *ptr,
  SbAtomic32 old_value,
  SbAtomic32 new_value);

// Atomically store new_value into *ptr, returning the previous value held in
// *ptr.  This routine implies no memory barriers.
SbAtomic32 SbAtomicNoBarrier_Exchange(
  volatile SbAtomic32 *ptr,
  SbAtomic32 new_value);

// Atomically increment *ptr by "increment".  Returns the new value of
// *ptr with the increment applied.  This routine implies no memory barriers.
SbAtomic32 SbAtomicNoBarrier_Increment(
  volatile SbAtomic32 *ptr,
  SbAtomic32 increment);

// Same as SbAtomicNoBarrier_Increment, but with a memory barrier.
SbAtomic32 SbAtomicBarrier_Increment(
  volatile SbAtomic32 *ptr,
  SbAtomic32 increment);

// These following lower-level operations are typically useful only to people
// implementing higher-level synchronization operations like spinlocks, mutexes,
// and condition-variables.  They combine CompareAndSwap(), a load, or a store
// with appropriate memory-ordering instructions.  "Acquire" operations ensure
// that no later memory access can be reordered ahead of the operation.
// "Release" operations ensure that no previous memory access can be reordered
// after the operation.  "Barrier" operations have both "Acquire" and "Release"
// semantics.  A SbAtomicMemoryBarrier() has "Barrier" semantics, but does no
// memory access.
SbAtomic32 SbAtomicAcquire_CompareAndSwap(
  volatile SbAtomic32 *ptr,
  SbAtomic32 old_value,
  SbAtomic32 new_value);
SbAtomic32 SbAtomicRelease_CompareAndSwap(
  volatile SbAtomic32 *ptr,
  SbAtomic32 old_value,
  SbAtomic32 new_value);

void SbAtomicMemoryBarrier();
void SbAtomicNoBarrier_Store(volatile SbAtomic32 *ptr, SbAtomic32 value);
void SbAtomicAcquire_Store(volatile SbAtomic32 *ptr, SbAtomic32 value);
void SbAtomicRelease_Store(volatile SbAtomic32 *ptr, SbAtomic32 value);

SbAtomic32 SbAtomicNoBarrier_Load(volatile const SbAtomic32 *ptr);
SbAtomic32 SbAtomicAcquire_Load(volatile const SbAtomic32 *ptr);
SbAtomic32 SbAtomicRelease_Load(volatile const SbAtomic32 *ptr);

// 64-bit atomic operations (only available on 64-bit processors).
#if SB_IS(64_BIT)
typedef int64_t SbAtomic64;

SbAtomic64 SbAtomicNoBarrier_CompareAndSwap64(
  volatile SbAtomic64 *ptr,
  SbAtomic64 old_value,
  SbAtomic64 new_value);
SbAtomic64 SbAtomicNoBarrier_Exchange64(
  volatile SbAtomic64 *ptr,
  SbAtomic64 new_value);
SbAtomic64 SbAtomicNoBarrier_Increment64(
  volatile SbAtomic64 *ptr,
  SbAtomic64 increment);
SbAtomic64 SbAtomicBarrier_Increment64(
  volatile SbAtomic64 *ptr,
  SbAtomic64 increment);
SbAtomic64 SbAtomicAcquire_CompareAndSwap64(
  volatile SbAtomic64 *ptr,
  SbAtomic64 old_value,
  SbAtomic64 new_value);
SbAtomic64 SbAtomicRelease_CompareAndSwap64(
  volatile SbAtomic64 *ptr,
  SbAtomic64 old_value,
  SbAtomic64 new_value);
void SbAtomicNoBarrier_Store64(volatile SbAtomic64 *ptr, SbAtomic64 value);
void SbAtomicAcquire_Store64(volatile SbAtomic64 *ptr, SbAtomic64 value);
void SbAtomicRelease_Store64(volatile SbAtomic64 *ptr, SbAtomic64 value);
SbAtomic64 SbAtomicNoBarrier_Load64(volatile const SbAtomic64 *ptr);
SbAtomic64 SbAtomicAcquire_Load64(volatile const SbAtomic64 *ptr);
SbAtomic64 SbAtomicRelease_Load64(volatile const SbAtomic64 *ptr);
#endif  // SB_IS(64_BIT)

// Include the platform definitions of these functions, which should be defined
// as inlined. This macro is defined on the command line by gyp.
#include STARBOARD_ATOMIC_INCLUDE

#endif  // STARBOARD_ATOMIC_H_
