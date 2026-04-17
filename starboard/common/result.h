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

#ifndef STARBOARD_COMMON_RESULT_H_
#define STARBOARD_COMMON_RESULT_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "starboard/common/expected.h"
#include "starboard/common/log.h"
#include "starboard/common/source_location.h"

namespace starboard {

// is_unique_ptr trait.
template <typename T>
struct is_unique_ptr : std::false_type {};
template <typename T, typename D>
struct is_unique_ptr<std::unique_ptr<T, D>> : std::true_type {};

template <typename T>
using Result = Expected<T, std::string>;

// A version of Result for pointer types that guarantees the success value is
// never null.
template <typename T>
class NonNullResult {
 public:
  static_assert(std::is_pointer<T>::value || is_unique_ptr<T>::value,
                "T must be a raw pointer or std::unique_ptr.");

  // Constructor for success value.
  // SB_CHECKs that the value is not null.
  template <
      typename U,
      typename = std::enable_if_t<
          std::is_convertible<U, T>::value &&
          !std::is_same<std::decay_t<U>, Unexpected<std::string>>::value &&
          !std::is_same<std::decay_t<U>, NonNullResult<T>>::value>>
  NonNullResult(U&& value) : result_(std::forward<U>(value)) {
    SB_CHECK(this->value() != nullptr) << "NonNullResult value cannot be null.";
  }

  // Constructor for failure value.
  NonNullResult(Unexpected<std::string> error) : result_(std::move(error)) {}

  // Forwarded methods
  constexpr bool has_value() const noexcept { return result_.has_value(); }
  constexpr explicit operator bool() const noexcept { return has_value(); }

  constexpr T& value() & noexcept { return result_.value(); }
  constexpr const T& value() const& noexcept { return result_.value(); }
  constexpr T&& value() && noexcept { return std::move(result_).value(); }

  constexpr std::string& error() & noexcept { return result_.error(); }
  constexpr const std::string& error() const& noexcept {
    return result_.error();
  }
  constexpr std::string&& error() && noexcept {
    return std::move(result_).error();
  }

  // Custom operators
  auto operator->() {
    if constexpr (std::is_pointer_v<T>) {
      return value();
    } else {  // is_unique_ptr
      return value().get();
    }
  }

  auto operator->() const {
    if constexpr (std::is_pointer_v<T>) {
      return value();
    } else {  // is_unique_ptr
      return value().get();
    }
  }

  auto& operator*() & { return *value(); }
  const auto& operator*() const& { return *value(); }

 private:
  Result<T> result_;
};

// Helper functions for creating success and failure Result values.
template <typename T>
inline Result<T> Success(T&& value) {
  return {std::forward<T>(value)};
}

inline Result<void> Success() {
  return {};
}

inline Unexpected<std::string> Failure(
    std::string_view error_message,
    const SourceLocation& location = SourceLocation::current()) {
  std::string msg;
  msg.reserve(error_message.size() + std::string_view(location.file()).size() +
              32);
  msg.append(error_message);
  msg.append(" at ");
  msg.append(location.file());
  msg.append(":");
  msg.append(std::to_string(location.line()));
  return Unexpected(std::move(msg));
}

}  // namespace starboard

#endif  // STARBOARD_COMMON_RESULT_H_
