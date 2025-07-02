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

#ifndef STARBOARD_COMMON_CHECK_H_
#define STARBOARD_COMMON_CHECK_H_

#include <ostream>

#include "starboard/common/log.h"

// Copied from base/check.h with modificatios, including
// - Keep minimal codes in CheckError, used by check_op.h.

namespace starboard::logging {

// Class used for raising a check error upon destruction.
class CheckError {
 public:
  // Used by CheckOp. Takes ownership of `log_message`.
  explicit CheckError(LogMessage* log_message) : log_message_(log_message) {}

  ~CheckError() { delete log_message_; }

  CheckError(const CheckError&) = delete;
  CheckError& operator=(const CheckError&) = delete;

  // Stream for adding optional details to the error message.
  std::ostream& stream() { return log_message_->stream(); }

  template <typename T>
  std::ostream& operator<<(T&& streamed_type) {
    return stream() << streamed_type;
  }

 private:
  LogMessage* const log_message_;
};

}  // namespace starboard::logging

#endif  // STARBOARD_COMMON_CHECK_H_
