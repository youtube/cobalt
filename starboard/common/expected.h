#ifndef STARBOARD_COMMON_RESULT_H_
#define STARBOARD_COMMON_RESULT_H_

#include <string>
#include <utility>
#include <variant>

#include "starboard/common/log.h"

// A simple wrapper for error messages to disambiguate from success types.
struct Unexpected {
  std::string error_message;

  Unexpected(std::string_view error_message) : error_message(error_message) {}
};

template <typename T>
class Expected {
 public:
  template <typename U,
            typename = std::enable_if_t<std::is_convertible<U, T>::value>>
  Expected(U&& value) : storage_(std::forward<U>(value)) {}
  Expected(Unexpected error) : storage_(std::move(error)) {}

  // Returns true if the Expected is a success.
  bool ok() const { return std::holds_alternative<T>(storage_); }

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

#endif  // STARBOARD_COMMON_RESULT_H_
