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

#include "cobalt/common/libc/time/icu_time_support.h"

#include <gtest/gtest.h>
#include <stdlib.h>
#include <time.h>

#include <string>

namespace cobalt {
namespace common {
namespace libc {
namespace time {

class IcuTimeSupportTest : public ::testing::Test {
 protected:
  void SetUp() override { original_tz_ = getenv("TZ"); }

  void TearDown() override {
    if (original_tz_) {
      setenv("TZ", original_tz_, 1);
    } else {
      unsetenv("TZ");
    }
  }

  const char* original_tz_ = nullptr;
};

TEST_F(IcuTimeSupportTest, GetPosixTimezoneGlobals) {
  setenv("TZ", "America/New_York", 1);
  IcuTimeSupport* time_support = IcuTimeSupport::GetInstance();

  long timezone_sec;
  int daylight;
  char* tzname[2];
  time_support->GetPosixTimezoneGlobals(timezone_sec, daylight, tzname);
  EXPECT_EQ(daylight, 1);

  setenv("TZ", "America/Los_Angeles", 1);
  time_support->GetPosixTimezoneGlobals(timezone_sec, daylight, tzname);
  EXPECT_EQ(daylight, 1);

  setenv("TZ", "UTC", 1);
  time_support->GetPosixTimezoneGlobals(timezone_sec, daylight, tzname);
  EXPECT_EQ(daylight, 0);
}

}  // namespace time
}  // namespace libc
}  // namespace common
}  // namespace cobalt
