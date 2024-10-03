// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard Atomic API
//
// Defines a set of atomic integer operations that can be used as lightweight
// synchronization or as building blocks for heavier synchronization primitives.
// Their use is very subtle and requires detailed understanding of the behavior
// of supported architectures, so their direct use is not recommended except
// when rigorously deemed absolutely necessary for performance reasons.

#ifndef STARBOARD_ATOMIC_H_
#define STARBOARD_ATOMIC_H_

#include "starboard/configuration.h"
#include "starboard/types.h"

#ifndef __cplusplus
#error "This header should not be included in C source code"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int8_t SbAtomic8;
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
static SbAtomic32 SbAtomicNoBarrier_CompareAndSwap(volatile SbAtomic32* ptr,
                                                   SbAtomic32 old_value,
                                                   SbAtomic32 new_value);

// Atomically store new_value into *ptr, returning the previous value held in
// *ptr.  This routine implies no memory barriers.
static SbAtomic32 SbAtomicNoBarrier_Exchange(volatile SbAtomic32* ptr,
                                             SbAtomic32 new_value);

// Atomically increment *ptr by "increment".  Returns the new value of
// *ptr with the increment applied.  This routine implies no memory barriers.
static SbAtomic32 SbAtomicNoBarrier_Increment(volatile SbAtomic32* ptr,
                                              SbAtomic32 increment);

// Same as SbAtomicNoBarrier_Increment, but with a memory barrier.
static SbAtomic32 SbAtomicBarrier_Increment(volatile SbAtomic32* ptr,
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
static SbAtomic32 SbAtomicAcquire_CompareAndSwap(volatile SbAtomic32* ptr,
                                                 SbAtomic32 old_value,
                                                 SbAtomic32 new_value);
static SbAtomic32 SbAtomicRelease_CompareAndSwap(volatile SbAtomic32* ptr,
                                                 SbAtomic32 old_value,
                                                 SbAtomic32 new_value);

static void SbAtomicMemoryBarrier();
static void SbAtomicNoBarrier_Store(volatile SbAtomic32* ptr, SbAtomic32 value);
static void SbAtomicAcquire_Store(volatile SbAtomic32* ptr, SbAtomic32 value);
static void SbAtomicRelease_Store(volatile SbAtomic32* ptr, SbAtomic32 value);

static SbAtomic32 SbAtomicNoBarrier_Load(volatile const SbAtomic32* ptr);
static SbAtomic32 SbAtomicAcquire_Load(volatile const SbAtomic32* ptr);
static SbAtomic32 SbAtomicRelease_Load(volatile const SbAtomic32* ptr);

// Overloaded functions for Atomic8.
static SbAtomic8 SbAtomicRelease_CompareAndSwap8(volatile SbAtomic8* ptr,
                                                 SbAtomic8 old_value,
                                                 SbAtomic8 new_value);
static void SbAtomicNoBarrier_Store8(volatile SbAtomic8* ptr, SbAtomic8 value);
static SbAtomic8 SbAtomicNoBarrier_Load8(volatile const SbAtomic8* ptr);

// 64-bit atomic operations (only available on 64-bit processors).
#if SB_HAS(64_BIT_ATOMICS)
typedef int64_t SbAtomic64;

static SbAtomic64 SbAtomicNoBarrier_CompareAndSwap64(volatile SbAtomic64* ptr,
                                                     SbAtomic64 old_value,
                                                     SbAtomic64 new_value);
static SbAtomic64 SbAtomicNoBarrier_Exchange64(volatile SbAtomic64* ptr,
                                               SbAtomic64 new_value);
static SbAtomic64 SbAtomicNoBarrier_Increment64(volatile SbAtomic64* ptr,
                                                SbAtomic64 increment);
static SbAtomic64 SbAtomicBarrier_Increment64(volatile SbAtomic64* ptr,
                                              SbAtomic64 increment);
static SbAtomic64 SbAtomicAcquire_CompareAndSwap64(volatile SbAtomic64* ptr,
                                                   SbAtomic64 old_value,
                                                   SbAtomic64 new_value);
static SbAtomic64 SbAtomicRelease_CompareAndSwap64(volatile SbAtomic64* ptr,
                                                   SbAtomic64 old_value,
                                                   SbAtomic64 new_value);
static void SbAtomicNoBarrier_Store64(volatile SbAtomic64* ptr,
                                      SbAtomic64 value);
static void SbAtomicAcquire_Store64(volatile SbAtomic64* ptr, SbAtomic64 value);
static void SbAtomicRelease_Store64(volatile SbAtomic64* ptr, SbAtomic64 value);
static SbAtomic64 SbAtomicNoBarrier_Load64(volatile const SbAtomic64* ptr);
static SbAtomic64 SbAtomicAcquire_Load64(volatile const SbAtomic64* ptr);
static SbAtomic64 SbAtomicRelease_Load64(volatile const SbAtomic64* ptr);
#endif  // SB_HAS(64_BIT_ATOMICS)

// Pointer-sized atomic operations. Forwards to either 32-bit or 64-bit
// functions as appropriate.
#if SB_SIZE_OF(POINTER) == 8
typedef SbAtomic64 SbAtomicPtr;
#else
typedef SbAtomic32 SbAtomicPtr;
#endif

static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicNoBarrier_CompareAndSwapPtr(volatile SbAtomicPtr* ptr,
                                    SbAtomicPtr old_value,
                                    SbAtomicPtr new_value) {
#if SB_SIZE_OF(POINTER) == 8
  return SbAtomicNoBarrier_CompareAndSwap64(ptr, old_value, new_value);
#else
  return SbAtomicNoBarrier_CompareAndSwap(ptr, old_value, new_value);
#endif
}

static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicNoBarrier_ExchangePtr(volatile SbAtomicPtr* ptr,
                              SbAtomicPtr new_value) {
#if SB_SIZE_OF(POINTER) == 8
  return SbAtomicNoBarrier_Exchange64(ptr, new_value);
#else
  return SbAtomicNoBarrier_Exchange(ptr, new_value);
#endif
}

static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicNoBarrier_IncrementPtr(volatile SbAtomicPtr* ptr,
                               SbAtomicPtr increment) {
#if SB_SIZE_OF(POINTER) == 8
  return SbAtomicNoBarrier_Increment64(ptr, increment);
#else
  return SbAtomicNoBarrier_Increment(ptr, increment);
#endif
}

static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicBarrier_IncrementPtr(volatile SbAtomicPtr* ptr, SbAtomicPtr increment) {
#if SB_SIZE_OF(POINTER) == 8
  return SbAtomicBarrier_Increment64(ptr, increment);
#else
  return SbAtomicBarrier_Increment(ptr, increment);
#endif
}

static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicAcquire_CompareAndSwapPtr(volatile SbAtomicPtr* ptr,
                                  SbAtomicPtr old_value,
                                  SbAtomicPtr new_value) {
#if SB_SIZE_OF(POINTER) == 8
  return SbAtomicAcquire_CompareAndSwap64(ptr, old_value, new_value);
#else
  return SbAtomicAcquire_CompareAndSwap(ptr, old_value, new_value);
#endif
}

static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicRelease_CompareAndSwapPtr(volatile SbAtomicPtr* ptr,
                                  SbAtomicPtr old_value,
                                  SbAtomicPtr new_value) {
#if SB_SIZE_OF(POINTER) == 8
  return SbAtomicRelease_CompareAndSwap64(ptr, old_value, new_value);
#else
  return SbAtomicRelease_CompareAndSwap(ptr, old_value, new_value);
#endif
}

static SB_C_FORCE_INLINE void SbAtomicNoBarrier_StorePtr(
    volatile SbAtomicPtr* ptr,
    SbAtomicPtr value) {
#if SB_SIZE_OF(POINTER) == 8
  SbAtomicNoBarrier_Store64(ptr, value);
#else
  SbAtomicNoBarrier_Store(ptr, value);
#endif
}

static SB_C_FORCE_INLINE void SbAtomicAcquire_StorePtr(
    volatile SbAtomicPtr* ptr,
    SbAtomicPtr value) {
#if SB_SIZE_OF(POINTER) == 8
  SbAtomicAcquire_Store64(ptr, value);
#else
  SbAtomicAcquire_Store(ptr, value);
#endif
}

static SB_C_FORCE_INLINE void SbAtomicRelease_StorePtr(
    volatile SbAtomicPtr* ptr,
    SbAtomicPtr value) {
#if SB_SIZE_OF(POINTER) == 8
  SbAtomicRelease_Store64(ptr, value);
#else
  SbAtomicRelease_Store(ptr, value);
#endif
}

static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicNoBarrier_LoadPtr(volatile const SbAtomicPtr* ptr) {
#if SB_SIZE_OF(POINTER) == 8
  return SbAtomicNoBarrier_Load64(ptr);
#else
  return SbAtomicNoBarrier_Load(ptr);
#endif
}

static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicAcquire_LoadPtr(volatile const SbAtomicPtr* ptr) {
#if SB_SIZE_OF(POINTER) == 8
  return SbAtomicAcquire_Load64(ptr);
#else
  return SbAtomicAcquire_Load(ptr);
#endif
}

static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicRelease_LoadPtr(volatile const SbAtomicPtr* ptr) {
#if SB_SIZE_OF(POINTER) == 8
  return SbAtomicRelease_Load64(ptr);
#else
  return SbAtomicRelease_Load(ptr);
#endif
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_ATOMIC_H_
