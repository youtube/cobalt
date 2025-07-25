// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABILITY_INTERNAL_TEMPLATES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABILITY_INTERNAL_TEMPLATES_H_

#include <cstdint>
#include <cstring>
#include <type_traits>

#include "base/template_util.h"

namespace blink {

namespace internal {

// Kinda conservative implementation of
// std::has_unique_object_representations<>. Perhaps not as conservative as we'd
// like.
//
// At a minimum, this predicate should require that the data type not contain
// internal padding since uninitialized padding bytes defeat the uniqueness of
// the representation. Trailing padding is allowed.
//
// Not checking <version> because we don't want to use the feature
// automatically. We should wait until C++-17 library functionality is
// explicitly allowed in Chromium.
template <typename T>
using has_unique_object_representations = std::is_arithmetic<T>;

// Calculate a digest of an object with a unique representation.
//
// In a perfect world, we should also require that this representation be
// portable or made to be portable. Such a transformation could be done, for
// example, by adopting a consistent byte ordering on all platforms.
//
// This function should only be invoked on a bare (sans qualifiers and
// references) type for the sake of simplicity.
//
// Should not be used as a primitive for manually constructing a unique
// representation. For such cases, use the byte-wise digest functions instead.
//
// Should not be used outside of the narrow use cases in this file.
//
// This implementation does not work for x86 extended precision floating point
// numbers. These are 80-bits wide, but in practice includes 6 bytes of padding
// in order to extend the size to 16 bytes. The extra bytes are uninitialized
// and will not contribute a stable digest.
template <
    typename T,
    typename std::enable_if_t<std::is_same<T, base::remove_cvref_t<T>>::value &&
                              std::is_trivially_copyable<T>::value &&
                              has_unique_object_representations<T>::value &&
                              sizeof(T) <= sizeof(int64_t)>* = nullptr>
constexpr int64_t DigestOfObjectRepresentation(T in) {
  // If |in| is small enough, the digest is itself. There's no point hashing
  // this value since the identity has all the properties we are looking for
  // in a digest.
  if (std::is_integral<T>::value && std::is_signed<T>::value)
    return in;

  if (std::is_integral<T>::value && sizeof(T) < sizeof(int64_t))
    return in;

  int64_t result = 0;
  std::memcpy(&result, &in, sizeof(in));
  return result;
}

}  // namespace internal

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABILITY_INTERNAL_TEMPLATES_H_
