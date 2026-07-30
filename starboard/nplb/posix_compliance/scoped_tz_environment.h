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

#ifndef STARBOARD_NPLB_POSIX_COMPLIANCE_SCOPED_TZ_ENVIRONMENT_H_
#define STARBOARD_NPLB_POSIX_COMPLIANCE_SCOPED_TZ_ENVIRONMENT_H_

#include <gtest/gtest.h>
#include <stdlib.h>

#include <optional>
#include <string>

namespace nplb {

// Helper class to manage the TZ environment variable for test isolation.
// Sets TZ in constructor, restores original TZ in destructor.
class ScopedTzEnvironment {
 public:
  explicit ScopedTzEnvironment(const char* new_tz_value) {
    const char* current_tz_env = getenv("TZ");
    if (current_tz_env != nullptr) {
      original_tz_value_ = current_tz_env;  // Store the original TZ string
    }

    if (new_tz_value != nullptr) {
      EXPECT_EQ(0, setenv("TZ", new_tz_value, 1))
          << "ScopedTzEnvironment: Failed to set TZ environment variable to \""
          << new_tz_value << "\"";
    } else {
      EXPECT_EQ(0, unsetenv("TZ"))
          << "ScopedTzEnvironment: Failed to unset TZ environment variable.";
    }
  }

  ~ScopedTzEnvironment() {
    if (original_tz_value_) {
      setenv("TZ", original_tz_value_->c_str(), 1);
    } else {
      unsetenv("TZ");
    }
  }

  ScopedTzEnvironment(const ScopedTzEnvironment&) = delete;
  ScopedTzEnvironment& operator=(const ScopedTzEnvironment&) = delete;

 private:
  std::optional<std::string> original_tz_value_;
};

}  // namespace nplb

#endif  // STARBOARD_NPLB_POSIX_COMPLIANCE_SCOPED_TZ_ENVIRONMENT_H_
