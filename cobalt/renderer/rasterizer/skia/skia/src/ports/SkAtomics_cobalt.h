// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKATOMICS_COBALT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKATOMICS_COBALT_H_

#include "base/atomicops.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/atomic_type_conversions.h"

static inline int32_t sk_atomic_inc(int32_t* addr) {
  return base::subtle::Barrier_AtomicIncrement(addr, 1) - 1;
}

#if defined(ARCH_CPU_64_BITS)
static inline int64_t sk_atomic_inc(int64_t* addr) {
  return base::subtle::Barrier_AtomicIncrement(addr, 1) - 1;
}
#endif

static inline int32_t sk_atomic_add(int32_t* addr, int32_t inc) {
  return base::subtle::Barrier_AtomicIncrement(addr, inc) - inc;
}

static inline int32_t sk_atomic_dec(int32_t* addr) {
  return base::subtle::Barrier_AtomicIncrement(addr, -1) + 1;
}

static inline void sk_membar_acquire__after_atomic_dec() {}

static inline bool sk_atomic_cas(int32_t* addr, int32_t before, int32_t after) {
  // This should issue a full memory barrier.  Since compare and swap is both
  // a read and a write, we issue a barrier before where a value may be written
  // to, and after a value has been read.  E.g. we don't want previous
  // instructions to be moved after this instruction and we don't want after
  // instructions to be moved before this one.
  SbAtomicMemoryBarrier();
  bool did_swap =
      base::subtle::NoBarrier_CompareAndSwap(addr, before, after) == before;
  SbAtomicMemoryBarrier();

  return did_swap;
}

static inline void* sk_atomic_cas(void** addr, void* before, void* after) {
  typedef AtomicTraits<void*>::atomic_type AtomicType;

  SbAtomicMemoryBarrier();
  void* previous_value =
      reinterpret_cast<void*>(base::subtle::NoBarrier_CompareAndSwap(
          reinterpret_cast<AtomicType*>(addr),
          reinterpret_cast<AtomicType>(before),
          reinterpret_cast<AtomicType>(after)));
  SbAtomicMemoryBarrier();

  return previous_value;
}

static inline void sk_membar_acquire__after_atomic_conditional_inc() {}

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKATOMICS_COBALT_H_
