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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKBARRIERS_COBALT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKBARRIERS_COBALT_H_

#include "base/atomicops.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/atomic_type_conversions.h"

template <typename T, bool compatible_with_atomics>
class SkAcquireReleaseWithAtomicCompatibility {};

template <typename T>
class SkAcquireReleaseWithAtomicCompatibility<T, false> {
 public:
  // If we don't have a Chromium Base atomics compatible type, issue full memory
  // barriers.
  static T DoAcquireLoad(T* ptr) {
    bool ret = *ptr;
    SbAtomicMemoryBarrier();
    return ret;
  }

  static void DoReleaseStore(T* ptr, T val) {
    SbAtomicMemoryBarrier();
    *ptr = val;
  }
};

template <typename T>
class SkAcquireReleaseWithAtomicCompatibility<T, true> {
 public:
  // If the type we are dealing with is compatible with Chromium's base
  // atomics library, take advantage of those specialized calls.
  static T DoAcquireLoad(T* ptr) {
    typedef typename AtomicTraits<T>::atomic_type AtomicType;
    AtomicType* as_atomic = reinterpret_cast<AtomicType*>(ptr);
    return AtomicTypeConversionPolicy<T>::FromAtomic(
        base::subtle::Acquire_Load(as_atomic));
  }

  static void DoReleaseStore(T* ptr, T val) {
    typedef typename AtomicTraits<T>::atomic_type AtomicType;
    AtomicType* as_atomic = reinterpret_cast<AtomicType*>(ptr);
    base::subtle::Release_Store(as_atomic,
                                AtomicTypeConversionPolicy<T>::ToAtomic(val));
  }
};

template <typename T>
class SkAcquireRelease : public SkAcquireReleaseWithAtomicCompatibility<
                             T, AtomicTraits<T>::compatible_with_atomics> {};

template <typename T>
T sk_acquire_load(T* ptr) {
  return SkAcquireRelease<T>::DoAcquireLoad(ptr);
}

template <typename T>
T sk_consume_load(T* ptr) {
  return sk_acquire_load(ptr);
}

template <typename T>
void sk_release_store(T* ptr, T val) {
  SkAcquireRelease<T>::DoReleaseStore(ptr, val);
}

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKBARRIERS_COBALT_H_
