// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WEBDRIVER_UTIL_COMMAND_RESULT_H_
#define COBALT_WEBDRIVER_UTIL_COMMAND_RESULT_H_

#include <string>

#include "base/optional.h"
#include "cobalt/webdriver/protocol/response.h"

namespace cobalt {
namespace webdriver {
namespace util {

// Helper class for returning a status code and result pair.
template <typename T>
class CommandResult {
 public:
  CommandResult() : status_code_(protocol::Response::kUnknownError) {}
  explicit CommandResult(protocol::Response::StatusCode status)
      : status_code_(status), can_retry_(false) {}
  CommandResult(protocol::Response::StatusCode status,
                const std::string& message)
      : status_code_(status), error_message_(message), can_retry_(false) {}
  CommandResult(protocol::Response::StatusCode status,
                const std::string& message, bool can_retry)
      : status_code_(status), error_message_(message), can_retry_(can_retry) {}
  explicit CommandResult(const T& result)
      : status_code_(protocol::Response::kSuccess),
        result_(result),
        can_retry_(false) {}

  bool is_success() const {
    return status_code_ == protocol::Response::kSuccess;
  }
  protocol::Response::StatusCode status_code() const { return status_code_; }
  const T& result() const { return result_.value(); }
  std::string error_message() const { return error_message_.value_or(""); }
  // Returns true if this result occurred due to a transient issue,
  // such as the WebModule going away. Retrying w/ a new WebModule
  // may succeed.
  bool can_retry() const { return can_retry_; }

 private:
  protocol::Response::StatusCode status_code_;
  base::Optional<T> result_;
  base::Optional<std::string> error_message_;
  bool can_retry_;
};

template <>
class CommandResult<void> {
 public:
  CommandResult() : status_code_(protocol::Response::kUnknownError) {}
  explicit CommandResult(protocol::Response::StatusCode status)
      : status_code_(status), can_retry_(false) {}
  CommandResult(protocol::Response::StatusCode status,
                const std::string& message)
      : status_code_(status), error_message_(message), can_retry_(false) {}
  CommandResult(protocol::Response::StatusCode status,
                const std::string& message, bool can_retry)
      : status_code_(status), error_message_(message), can_retry_(can_retry) {}

  bool is_success() const {
    return status_code_ == protocol::Response::kSuccess;
  }
  protocol::Response::StatusCode status_code() const { return status_code_; }
  std::string error_message() const { return error_message_.value_or(""); }
  // Returns true if this result occurred due to a transient issue,
  // such as the WebModule going away. Retrying w/ a new WebModule
  // may succeed.
  bool can_retry() const { return can_retry_; }

 private:
  protocol::Response::StatusCode status_code_;
  base::Optional<std::string> error_message_;
  bool can_retry_;
};

}  // namespace util
}  // namespace webdriver
}  // namespace cobalt

#endif  // COBALT_WEBDRIVER_UTIL_COMMAND_RESULT_H_
