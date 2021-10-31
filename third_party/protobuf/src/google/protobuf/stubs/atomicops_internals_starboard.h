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

// This file is an internal atomic implementation, use base/atomicops.h instead.

// This is the Starboard implementation, which defers all specific
// implementation decisions for atomic operations to the Starboard port.

#ifndef GOOGLE_PROTOBUF_ATOMICOPS_INTERNALS_STARBOARD_H_
#define GOOGLE_PROTOBUF_ATOMICOPS_INTERNALS_STARBOARD_H_

#include "starboard/atomic.h"

namespace google {
namespace protobuf {
namespace internal {

inline Atomic32 NoBarrier_CompareAndSwap(volatile Atomic32* ptr,
                                         Atomic32 old_value,
                                         Atomic32 new_value) {
  return SbAtomicNoBarrier_CompareAndSwap(ptr, old_value, new_value);
}

inline Atomic32 NoBarrier_AtomicExchange(volatile Atomic32* ptr,
                                         Atomic32 new_value) {
  return SbAtomicNoBarrier_Exchange(ptr, new_value);
}

inline Atomic32 NoBarrier_AtomicIncrement(volatile Atomic32* ptr,
                                          Atomic32 increment) {
  return SbAtomicNoBarrier_Increment(ptr, increment);
}

inline Atomic32 Barrier_AtomicIncrement(volatile Atomic32* ptr,
                                        Atomic32 increment) {
  return SbAtomicBarrier_Increment(ptr, increment);
}

inline Atomic32 Acquire_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  return SbAtomicAcquire_CompareAndSwap(ptr, old_value, new_value);
}

inline Atomic32 Release_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  return SbAtomicRelease_CompareAndSwap(ptr, old_value, new_value);
}

inline void NoBarrier_Store(volatile Atomic32* ptr, Atomic32 value) {
  SbAtomicNoBarrier_Store(ptr, value);
}

inline void MemoryBarrier() {
  SbAtomicMemoryBarrier();
}

inline void Acquire_Store(volatile Atomic32* ptr, Atomic32 value) {
  SbAtomicAcquire_Store(ptr, value);
}

inline void Release_Store(volatile Atomic32* ptr, Atomic32 value) {
  SbAtomicRelease_Store(ptr, value);
}

inline Atomic32 NoBarrier_Load(volatile const Atomic32* ptr) {
  return SbAtomicNoBarrier_Load(ptr);
}

inline Atomic32 Acquire_Load(volatile const Atomic32* ptr) {
  return SbAtomicAcquire_Load(ptr);
}

inline Atomic32 Release_Load(volatile const Atomic32* ptr) {
  return SbAtomicRelease_Load(ptr);
}

#if SB_IS(64_BIT)
inline Atomic64 NoBarrier_CompareAndSwap(volatile Atomic64* ptr,
                                         Atomic64 old_value,
                                         Atomic64 new_value) {
  return SbAtomicNoBarrier_CompareAndSwap64(ptr, old_value, new_value);
}

inline Atomic64 NoBarrier_AtomicExchange(volatile Atomic64* ptr,
                                         Atomic64 new_value) {
  return SbAtomicNoBarrier_Exchange64(ptr, new_value);
}

inline Atomic64 NoBarrier_AtomicIncrement(volatile Atomic64* ptr,
                                          Atomic64 increment) {
  return SbAtomicNoBarrier_Increment64(ptr, increment);
}

inline Atomic64 Barrier_AtomicIncrement(volatile Atomic64* ptr,
                                        Atomic64 increment) {
  return SbAtomicBarrier_Increment64(ptr, increment);
}

inline Atomic64 Acquire_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  return SbAtomicAcquire_CompareAndSwap64(ptr, old_value, new_value);
}

inline Atomic64 Release_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  return SbAtomicRelease_CompareAndSwap64(ptr, old_value, new_value);
}

inline void NoBarrier_Store(volatile Atomic64* ptr, Atomic64 value) {
  SbAtomicNoBarrier_Store64(ptr, value);
}

inline void Acquire_Store(volatile Atomic64* ptr, Atomic64 value) {
  SbAtomicAcquire_Store64(ptr, value);
}

inline void Release_Store(volatile Atomic64* ptr, Atomic64 value) {
  SbAtomicRelease_Store64(ptr, value);
}

inline Atomic64 NoBarrier_Load(volatile const Atomic64* ptr) {
  return SbAtomicNoBarrier_Load64(ptr);
}

inline Atomic64 Acquire_Load(volatile const Atomic64* ptr) {
  return SbAtomicAcquire_Load64(ptr);
}

inline Atomic64 Release_Load(volatile const Atomic64* ptr) {
  return SbAtomicRelease_Load64(ptr);
}
#endif  // SB_IS(64_BIT)

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_ATOMICOPS_INTERNALS_STARBOARD_H_
