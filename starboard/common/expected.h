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

#include <utility>
#include <variant>

#include "starboard/common/log.h"

namespace starboard {

template <typename E>
class Unexpected {
 public:
  explicit Unexpected(E error) : error_(std::move(error)) {}

  E& error() & { return error_; }
  const E& error() const& { return error_; }
  E&& error() && { return std::move(error_); }

 private:
  E error_;
};

template <typename T, typename E>
class Expected {
 public:
  template <typename U,
            typename = std::enable_if_t<
                std::is_convertible<U, T>::value &&
                !std::is_same<std::decay_t<U>, Unexpected<E>>::value>>
  Expected(U&& value) : storage_(std::forward<U>(value)) {}
  Expected(Unexpected<E> unexpected)
      : storage_(std::in_place_type<E>, std::move(unexpected.error())) {}

  bool has_value() const { return std::holds_alternative<T>(storage_); }
  explicit operator bool() const { return has_value(); }

  T& value() & {
    SB_CHECK(has_value());
    return std::get<T>(storage_);
  }
  const T& value() const& {
    SB_CHECK(has_value());
    return std::get<T>(storage_);
  }
  T&& value() && {
    SB_CHECK(has_value());
    return std::move(std::get<T>(storage_));
  }

  E& error() & {
    SB_CHECK(!has_value());
    return std::get<E>(storage_);
  }
  const E& error() const& {
    SB_CHECK(!has_value());
    return std::get<E>(storage_);
  }
  E&& error() && {
    SB_CHECK(!has_value());
    return std::move(std::get<E>(storage_));
  }

  T* operator->() {
    SB_CHECK(has_value());
    return &std::get<T>(storage_);
  }
  const T* operator->() const {
    SB_CHECK(has_value());
    return &std::get<T>(storage_);
  }

  T& operator*() & {
    SB_CHECK(has_value());
    return std::get<T>(storage_);
  }
  const T& operator*() const& {
    SB_CHECK(has_value());
    return std::get<T>(storage_);
  }
  T&& operator*() && {
    SB_CHECK(has_value());
    return std::move(std::get<T>(storage_));
  }

 private:
  std::variant<T, E> storage_;
};

template <typename E>
class Expected<void, E> {
 public:
  Expected() : storage_() {}
  Expected(Unexpected<E> unexpected)
      : storage_(std::in_place_type<E>, std::move(unexpected.error())) {}

  bool has_value() const {
    return std::holds_alternative<std::monostate>(storage_);
  }
  explicit operator bool() const { return has_value(); }

  void value() const { SB_CHECK(has_value()); }

  E& error() & {
    SB_CHECK(!has_value());
    return std::get<E>(storage_);
  }
  const E& error() const& {
    SB_CHECK(!has_value());
    return std::get<E>(storage_);
  }
  E&& error() && {
    SB_CHECK(!has_value());
    return std::move(std::get<E>(storage_));
  }

 private:
  std::variant<std::monostate, E> storage_;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_EXPECTED_H_
