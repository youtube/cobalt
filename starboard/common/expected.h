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

  bool has_value() const { return has_value_; }
  explicit operator bool() const { return has_value_; }

  T& value() & {
    SB_CHECK(has_value());
    return storage_.value_;
  }
  const T& value() const& {
    SB_CHECK(has_value());
    return storage_.value_;
  }
  T&& value() && {
    SB_CHECK(has_value());
    return std::move(storage_.value_);
  }

  E& error() & {
    SB_CHECK(!has_value());
    return storage_.error_;
  }
  const E& error() const& {
    SB_CHECK(!has_value());
    return storage_.error_;
  }
  E&& error() && {
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

  bool has_value() const { return !error_.has_value(); }
  explicit operator bool() const { return has_value(); }

  void value() const { SB_CHECK(has_value()); }

  E& error() & {
    SB_CHECK(!has_value());
    return *error_;
  }
  const E& error() const& {
    SB_CHECK(!has_value());
    return *error_;
  }
  E&& error() && {
    SB_CHECK(!has_value());
    return std::move(*error_);
  }

 private:
  std::optional<E> error_;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_EXPECTED_H_
