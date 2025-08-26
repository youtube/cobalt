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

#ifndef STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_TIMEZONE_TEST_HELPER_H_
#define STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_TIMEZONE_TEST_HELPER_H_

#include <gtest/gtest.h>
#include <stdlib.h>
#include <time.h>
#include <optional>
#include <string>
#include <type_traits>

namespace starboard {
namespace nplb {

// The declarations for tzname, timezone, and daylight are provided by <time.h>.
// We use static_assert to ensure they have the expected types as defined by
// POSIX.

// POSIX defines tzname as "char *tzname[]", but both musl and glibc define it
// as "char *tzname[2]". This check allows either.
static_assert(std::is_same_v<decltype(tzname), char* [2]> ||
                  std::is_same_v<decltype(tzname), char*[]>,
              "Type of tzname must be 'char* [2]' or 'char* []'.");

static_assert(std::is_same_v<decltype(timezone), long>,
              "Type of timezone must be 'long'.");
static_assert(std::is_same_v<decltype(daylight), int>,
              "Type of daylight must be 'int'.");

// Number of seconds in an hour.
constexpr int kSecondsInHour = 3600;
// Number of seconds in a minute.
constexpr int kSecondsInMinute = 60;

// Helper class to manage the TZ environment variable for test isolation.
// Sets TZ in constructor, restores original TZ in destructor.
// Calls tzset() after each change to TZ.
class ScopedTZ {
 public:
  ScopedTZ(const char* new_tz_value) {
    const char* current_tz_env = getenv("TZ");
    if (current_tz_env != nullptr) {
      original_tz_value_ = current_tz_env;  // Store the original TZ string
    }

    if (new_tz_value != nullptr) {
      EXPECT_EQ(0, setenv("TZ", new_tz_value, 1))
          << "ScopedTZ: Failed to set TZ environment variable to \""
          << new_tz_value << "\"";
    } else {
      EXPECT_EQ(0, unsetenv("TZ"))
          << "ScopedTZ: Failed to unset TZ environment variable.";
    }
    tzset();
  }

  ~ScopedTZ() {
    if (original_tz_value_) {
      setenv("TZ", original_tz_value_->c_str(), 1);
    } else {
      unsetenv("TZ");
    }
    tzset();
  }

  ScopedTZ(const ScopedTZ&) = delete;
  ScopedTZ& operator=(const ScopedTZ&) = delete;

 private:
  std::optional<std::string> original_tz_value_;
};

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_TIMEZONE_TEST_HELPER_H_
