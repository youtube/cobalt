// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_SPAN_H_
#define STARBOARD_COMMON_SPAN_H_

#include <cstddef>
#include <type_traits>

// A simple, lightweight alternative to C++20's std::span.
// Designed to pass a pointer and size together as a single object.
// Used because Cobalt currently targets C++17 where std::span is not available.
namespace starboard {

template <class T>
class Span {
 public:
  constexpr Span() = default;
  constexpr Span(T* data, size_t size) : data_(data), size_(size) {}

  // Permits safe qualification conversions (e.g. Span<T> to Span<const T>).
  template <class U,
            class = std::enable_if_t<std::is_convertible_v<U (*)[], T (*)[]>>>
  constexpr Span(const Span<U>& other)
      : data_(other.data()), size_(other.size()) {}

  // Member functions are named to match C++20's std::span for future
  // compatibility.
  constexpr T* data() const { return data_; }
  constexpr size_t size() const { return size_; }
  constexpr bool empty() const { return size_ == 0; }

 private:
  T* data_ = nullptr;
  size_t size_ = 0;
};

// Helper function analogous to absl::MakeSpan or base::make_span.
template <class T>
constexpr Span<T> MakeSpan(T* data, size_t size) {
  return Span<T>(data, size);
}

}  // namespace starboard

#endif  // STARBOARD_COMMON_SPAN_H_
