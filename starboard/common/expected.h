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

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "starboard/common/log.h"
#include "starboard/common/source_location.h"

namespace starboard {

// is_unique_ptr trait.
template <typename T>
struct is_unique_ptr : std::false_type {};
template <typename T, typename D>
struct is_unique_ptr<std::unique_ptr<T, D>> : std::true_type {};

// This file provides a simple implementation of a value-or-error type, similar
// to Chromium's `base::expected` or C++23's `std::expected`. It is designed to
// be a lightweight way to handle return values from functions that can either
// succeed with a value or fail with an error message.
//
// Example Usage:
//
//   // Function that returns a value or an error.
//   Expected<int> ParseInt(const std::string& s) {
//     try {
//       return std::stoi(s);
//     } catch (const std::invalid_argument&) {
//       return Failure("Invalid integer format");
//     }
//   }
//
//   // Function that returns void or an error.
//   Expected<void> DoSomething() {
//     if (/* condition */) {
//       return Success();
//     }
//     return Failure("Failed to do something");
//   }
//
//   void Process() {
//     Expected<int> result = ParseInt("123");
//     if (result) {
//       SB_LOG(INFO) << "Parsed value: " << result.value();
//     } else {
//       SB_LOG(ERROR) << "Error: " << result.error_message();
//     }
//
//     Expected<void> void_result = DoSomething();
//     if (!void_result) {
//       SB_LOG(ERROR) << "DoSomething failed: " << void_result.error_message();
//     }
//   }
//
// For improved readability, you can also use the helper functions:
//
//   Expected<int> GetPositive(int n) {
//     if (n > 0) {
//       // `return Success(n)` would also work.
//       return n;
//     }
//     return Failure("Number must be positive");
//   }
//
//   Expected<void> CheckSystem() {
//     if (/* system is ok */) {
//       return Success();
//     }
//     return Failure("System check failed");
//   }

// A simple wrapper for error messages to disambiguate from success types.
struct Unexpected {
  std::string error_message;

  explicit Unexpected(std::string_view error_message)
      : error_message(error_message) {}
};

template <typename T>
class Expected {
 public:
  template <typename U,
            typename = std::enable_if_t<
                std::is_convertible<U, T>::value &&
                !std::is_same<std::decay_t<U>, Unexpected>::value>>
  Expected(U&& value) : storage_(std::forward<U>(value)) {}
  Expected(Unexpected error) : storage_(std::move(error)) {}

  // Returns true if the Expected is a success.
  bool ok() const { return std::holds_alternative<T>(storage_); }

  explicit operator bool() const { return ok(); }

  // Returns the contained value.
  // This should only be called when ok() is true.
  T& value() & {
    SB_CHECK(ok());
    return std::get<T>(storage_);
  }
  const T& value() const& {
    SB_CHECK(ok());
    return std::get<T>(storage_);
  }
  T&& value() && {
    SB_CHECK(ok());
    return std::move(std::get<T>(storage_));
  }

  T* operator->() {
    SB_CHECK(ok());
    return &std::get<T>(storage_);
  }

  const T* operator->() const {
    SB_CHECK(ok());
    return &std::get<T>(storage_);
  }

  T& operator*() & {
    SB_CHECK(ok());
    return std::get<T>(storage_);
  }

  const T& operator*() const& {
    SB_CHECK(ok());
    return std::get<T>(storage_);
  }

  T&& operator*() && {
    SB_CHECK(ok());
    return std::move(std::get<T>(storage_));
  }

  // Returns the error message.
  // This should only be called when ok() is false.
  const std::string& error_message() const& {
    SB_CHECK(!ok());
    return std::get<Unexpected>(storage_).error_message;
  }
  std::string&& error_message() && {
    SB_CHECK(!ok());
    return std::move(std::get<Unexpected>(storage_).error_message);
  }

 private:
  std::variant<T, Unexpected> storage_;
};

// Specialization for void.
template <>
class Expected<void> {
 public:
  Expected() : storage_() {}
  Expected(Unexpected error) : storage_(std::move(error)) {}

  // Returns true if the Expected is a success.
  bool ok() const { return std::holds_alternative<std::monostate>(storage_); }

  explicit operator bool() const { return ok(); }

  // No-op for Expected<void>.
  // This should only be called when ok() is true.
  void value() & { SB_CHECK(ok()); }
  void value() const& { SB_CHECK(ok()); }
  void value() && { SB_CHECK(ok()); }

  // Returns the error message.
  // This should only be called when ok() is false.
  const std::string& error_message() const& {
    SB_CHECK(!ok());
    return std::get<Unexpected>(storage_).error_message;
  }
  std::string&& error_message() && {
    SB_CHECK(!ok());
    return std::move(std::get<Unexpected>(storage_).error_message);
  }

 private:
  std::variant<std::monostate, Unexpected> storage_;
};

// A version of Expected for pointer types that guarantees the success value is
// never null.
template <typename T>
class ExpectedNonNull {
 public:
  static_assert(std::is_pointer<T>::value || is_unique_ptr<T>::value,
                "T must be a raw pointer or std::unique_ptr.");

  // Constructor for success value.
  // SB_CHECKs that the value is not null.
  template <typename U,
            typename = std::enable_if_t<
                std::is_convertible<U, T>::value &&
                !std::is_same<std::decay_t<U>, Unexpected>::value>>
  ExpectedNonNull(U&& value) : storage_(std::forward<U>(value)) {
    SB_CHECK(std::get<T>(storage_) != nullptr)
        << "ExpectedNonNull value cannot be null.";
  }

  // Constructor for failure value.
  ExpectedNonNull(Unexpected error) : storage_(std::move(error)) {}

  bool ok() const { return std::holds_alternative<T>(storage_); }
  explicit operator bool() const { return ok(); }

  T& value() & {
    SB_CHECK(ok());
    return std::get<T>(storage_);
  }
  const T& value() const& {
    SB_CHECK(ok());
    return std::get<T>(storage_);
  }
  T&& value() && {
    SB_CHECK(ok());
    return std::move(std::get<T>(storage_));
  }

  auto operator->() {
    SB_CHECK(ok());
    if constexpr (std::is_pointer_v<T>) {
      return std::get<T>(storage_);
    } else {  // is_unique_ptr
      return std::get<T>(storage_).get();
    }
  }

  auto operator->() const {
    SB_CHECK(ok());
    if constexpr (std::is_pointer_v<T>) {
      return std::get<T>(storage_);
    } else {  // is_unique_ptr
      return std::get<T>(storage_).get();
    }
  }

  auto& operator*() & {
    SB_CHECK(ok());
    return *std::get<T>(storage_);
  }

  const auto& operator*() const& {
    SB_CHECK(ok());
    return *std::get<T>(storage_);
  }

  const std::string& error_message() const& {
    SB_CHECK(!ok());
    return std::get<Unexpected>(storage_).error_message;
  }
  std::string&& error_message() && {
    SB_CHECK(!ok());
    return std::move(std::get<Unexpected>(storage_).error_message);
  }

 private:
  std::variant<T, Unexpected> storage_;
};

// Helper functions for creating success and failure Expected values.
template <typename T>
inline Expected<T> Success(T&& value) {
  return {std::forward<T>(value)};
}

inline Expected<void> Success() {
  return {};
}

inline Unexpected Failure(
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

#endif  // STARBOARD_COMMON_EXPECTED_H_
