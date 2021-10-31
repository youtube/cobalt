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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_ATOMIC_TYPE_CONVERSIONS_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_ATOMIC_TYPE_CONVERSIONS_H_

#include "base/atomicops.h"

// AtomicTraitsBySize determines attributes about a type based on its size,
// sucha as whether or not it is compatible with Chromium's base atomics
// library, and if so, what the corresponding atomic type is.
template <int size>
struct AtomicTraitsBySize {
  static const bool compatible_with_atomics = false;
};

template <>
struct AtomicTraitsBySize<4> {
  static const bool compatible_with_atomics = true;
  typedef base::subtle::Atomic32 atomic_type;
};

#if defined(ARCH_CPU_64_BITS)
template <>
struct AtomicTraitsBySize<8> {
  static const bool compatible_with_atomics = true;
  typedef base::subtle::Atomic64 atomic_type;
};
#endif

// AtomicTraits wraps and has a more natural interface than AtomicTraitsBySize.
template <typename T>
struct AtomicTraits : public AtomicTraitsBySize<sizeof(T)> {};

// Classes for dealing with converting from any given type to its corresponding
// atomic type.
template <typename T>
struct AtomicTypeConversionPolicy {
  typedef typename AtomicTraits<T>::atomic_type type;

  static T FromAtomic(const type& value) { return static_cast<T>(value); }

  static type ToAtomic(const T& value) { return static_cast<type>(value); }
};

// For pointers, we must use reinterpret_cast instead of static_cast.
template <>
struct AtomicTypeConversionPolicy<void*> {
  typedef AtomicTraits<void*>::atomic_type type;

  static void* FromAtomic(const type& value) {
    return reinterpret_cast<void*>(value);
  }

  static type ToAtomic(void* value) { return reinterpret_cast<type>(value); }
};

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_ATOMIC_TYPE_CONVERSIONS_H_
