// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_EXPECTED_H_
#define STARBOARD_COMMON_EXPECTED_H_

#include <optional>
#include <type_traits>
#include <utility>

#include "starboard/common/log.h"

namespace starboard {

template <typename E>
class Unexpected {
 public:
  constexpr explicit Unexpected(E error) : error_(std::move(error)) {}

  constexpr E& error() & noexcept { return error_; }
  constexpr const E& error() const& noexcept { return error_; }
  constexpr E&& error() && noexcept { return std::move(error_); }
  constexpr const E&& error() const&& noexcept { return std::move(error_); }

 private:
  E error_;
};

// A lightweight class modeled after C++23's std::expected. It offers a
// robust and expressive way to handle function return values that can hold
// either a success value of type T or an error of type E.
//
// It is a vocabulary type for functions that can fail, providing a clear and
// explicit way to handle errors instead of using exceptions or out-of-band
// error codes (e.g., returning a null pointer or a magic number).
//
// Example Usage:
//   Expected<int, std::string> ParseInt(const std::string& s) {
//     try {
//       return std::stoi(s);
//     } catch (const std::exception& e) {
//       // Use the Unexpected helper to construct an error state.
//       return Unexpected(std::string(e.what()));
//     }
//   }
//
//   auto result = ParseInt("123");
//   if (result) {  // or result.has_value()
//     UseValue(*result); // or result.value()
//   } else {
//     HandleError(result.error());
//   }
//
// NOTE: This implementation can be replaced with C++ 23's `std::expected`, when
// it becomes available in this codebase.
template <typename T, typename E>
class Expected {
 public:
  // Provides a constrained templated constructor for copying the success value.
  // This is implemented as a template with `std::enable_if` to ensure it is
  // only instantiated for copy-constructible types. This is the C++17
  // standard-compliant way to prevent compilation errors when `Expected` is
  // used with move-only types like `std::unique_ptr`.
  template <typename U = T,
            std::enable_if_t<std::is_copy_constructible<U>::value, int> = 0>
  Expected(const T& value) : has_value_(true) {
    new (&storage_.value_) T(value);
  }

  // Explicitly provides a non-templated move constructor for the success value
  // T. This is the best match for rvalue arguments (e.g., 'return T{};' or
  // 'return std::move(value);'), guaranteeing an efficient move construction of
  // the stored value.
  Expected(T&& value) : has_value_(true) {
    new (&storage_.value_) T(std::move(value));
  }

  template <typename U,
            typename = std::enable_if_t<
                std::is_convertible<U, T>::value &&
                !std::is_same<std::decay_t<U>, T>::value &&
                !std::is_same<std::decay_t<U>, Unexpected<E>>::value &&
                !std::is_same<std::decay_t<U>, Expected<T, E>>::value>>
  Expected(U&& value) : has_value_(true) {
    new (&storage_.value_) T(std::forward<U>(value));
  }

  Expected(Unexpected<E> unexpected) : has_value_(false) {
    new (&storage_.error_) E(std::move(unexpected.error()));
  }

  Expected(const Expected& other) : has_value_(other.has_value_) {
    if (has_value_) {
      new (&storage_.value_) T(other.storage_.value_);
    } else {
      new (&storage_.error_) E(other.storage_.error_);
    }
  }

  Expected(Expected&& other) : has_value_(other.has_value_) {
    if (has_value_) {
      new (&storage_.value_) T(std::move(other.storage_.value_));
    } else {
      new (&storage_.error_) E(std::move(other.storage_.error_));
    }
  }

  ~Expected() {
    if (has_value_) {
      storage_.value_.~T();
    } else {
      storage_.error_.~E();
    }
  }

  Expected& operator=(const Expected& other) {
    if (this == &other) {
      return *this;
    }
    if (has_value_ && other.has_value_) {
      storage_.value_ = other.storage_.value_;
    } else if (!has_value_ && !other.has_value_) {
      storage_.error_ = other.storage_.error_;
    } else if (has_value_ && !other.has_value_) {
      storage_.value_.~T();
      new (&storage_.error_) E(other.storage_.error_);
    } else {  // !has_value_ && other.has_value_
      storage_.error_.~E();
      new (&storage_.value_) T(other.storage_.value_);
    }
    has_value_ = other.has_value_;
    return *this;
  }

  Expected& operator=(Expected&& other) {
    if (this == &other) {
      return *this;
    }
    if (has_value_ && other.has_value_) {
      storage_.value_ = std::move(other.storage_.value_);
    } else if (!has_value_ && !other.has_value_) {
      storage_.error_ = std::move(other.storage_.error_);
    } else if (has_value_ && !other.has_value_) {
      storage_.value_.~T();
      new (&storage_.error_) E(std::move(other.storage_.error_));
    } else {  // !has_value_ && other.has_value_
      storage_.error_.~E();
      new (&storage_.value_) T(std::move(other.storage_.value_));
    }
    has_value_ = other.has_value_;
    return *this;
  }

  constexpr bool has_value() const noexcept { return has_value_; }
  constexpr explicit operator bool() const noexcept { return has_value_; }

  constexpr T& value() & noexcept {
    SB_CHECK(has_value());
    return storage_.value_;
  }
  constexpr const T& value() const& noexcept {
    SB_CHECK(has_value());
    return storage_.value_;
  }
  constexpr T&& value() && noexcept {
    SB_CHECK(has_value());
    return std::move(storage_.value_);
  }
  constexpr const T&& value() const&& noexcept {
    SB_CHECK(has_value());
    return std::move(storage_.value_);
  }

  constexpr E& error() & noexcept {
    SB_CHECK(!has_value());
    return storage_.error_;
  }
  constexpr const E& error() const& noexcept {
    SB_CHECK(!has_value());
    return storage_.error_;
  }
  constexpr E&& error() && noexcept {
    SB_CHECK(!has_value());
    return std::move(storage_.error_);
  }
  constexpr const E&& error() const&& noexcept {
    SB_CHECK(!has_value());
    return std::move(storage_.error_);
  }

  T* operator->() {
    SB_CHECK(has_value());
    return &storage_.value_;
  }
  const T* operator->() const {
    SB_CHECK(has_value());
    return &storage_.value_;
  }

  T& operator*() & {
    SB_CHECK(has_value());
    return storage_.value_;
  }
  const T& operator*() const& {
    SB_CHECK(has_value());
    return storage_.value_;
  }
  T&& operator*() && {
    SB_CHECK(has_value());
    return std::move(storage_.value_);
  }
  const T&& operator*() const&& {
    SB_CHECK(has_value());
    return std::move(storage_.value_);
  }

 private:
  union Storage {
    Storage() {}
    ~Storage() {}
    T value_;
    E error_;
  } storage_;

  bool has_value_;
};

template <typename E>
class Expected<void, E> {
 public:
  Expected() : error_(std::nullopt) {}
  Expected(Unexpected<E> unexpected) : error_(std::move(unexpected.error())) {}

  constexpr bool has_value() const noexcept { return !error_.has_value(); }
  constexpr explicit operator bool() const noexcept { return has_value(); }

  constexpr void value() const noexcept { SB_CHECK(has_value()); }

  constexpr E& error() & noexcept {
    SB_CHECK(!has_value());
    return *error_;
  }
  constexpr const E& error() const& noexcept {
    SB_CHECK(!has_value());
    return *error_;
  }
  constexpr E&& error() && noexcept {
    SB_CHECK(!has_value());
    return std::move(*error_);
  }
  constexpr const E&& error() const&& noexcept {
    SB_CHECK(!has_value());
    return std::move(*error_);
  }

 private:
  std::optional<E> error_;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_EXPECTED_H_
