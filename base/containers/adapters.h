// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_ADAPTERS_H_
#define BASE_CONTAINERS_ADAPTERS_H_

#include <stddef.h>

#include <iterator>
#include <utility>

#include "base/memory/raw_ptr_exclusion.h"

namespace base {

namespace internal {

// Internal adapter class for implementing base::Reversed.
template <typename T>
class ReversedAdapter {
 public:
  using Iterator = decltype(std::rbegin(std::declval<T&>()));

  explicit ReversedAdapter(T& t) : t_(t) {}
  ReversedAdapter(const ReversedAdapter& ra) : t_(ra.t_) {}
  ReversedAdapter& operator=(const ReversedAdapter&) = delete;

  Iterator begin() const { return std::rbegin(t_); }
  Iterator end() const { return std::rend(t_); }

 private:
  // Not a raw_ref<...> for performance reasons: on-stack pointer.
  // It is only used inside for loops. Ideally, the container being iterated
  // over should be the one held via a raw_ref/raw_ptrs.
  RAW_PTR_EXCLUSION T& t_;
};

}  // namespace internal

// Reversed returns a container adapter usable in a range-based "for" statement
// for iterating a reversible container in reverse order.
//
// Example:
//
//   std::vector<int> v = ...;
//   for (int i : base::Reversed(v)) {
//     // iterates through v from back to front
//   }
template <typename T>
internal::ReversedAdapter<T> Reversed(T& t) {
  return internal::ReversedAdapter<T>(t);
}

}  // namespace base

#endif  // BASE_CONTAINERS_ADAPTERS_H_
