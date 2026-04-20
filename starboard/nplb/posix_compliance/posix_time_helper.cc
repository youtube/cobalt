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

#include "starboard/nplb/posix_compliance/posix_time_helper.h"

#include <gtest/gtest.h>
#include <time.h>

namespace nplb {

void ExpectTmEqual(const struct tm& actual,
                   const struct tm& expected,
                   const std::string& context) {
  EXPECT_EQ(actual.tm_sec, expected.tm_sec)
      << "tm_sec mismatch for " << context;
  EXPECT_EQ(actual.tm_min, expected.tm_min)
      << "tm_min mismatch for " << context;
  EXPECT_EQ(actual.tm_hour, expected.tm_hour)
      << "tm_hour mismatch for " << context;
  EXPECT_EQ(actual.tm_mday, expected.tm_mday)
      << "tm_mday mismatch for " << context;
  EXPECT_EQ(actual.tm_mon, expected.tm_mon)
      << "tm_mon mismatch for " << context;
  EXPECT_EQ(actual.tm_year, expected.tm_year)
      << "tm_year mismatch for " << context;
  EXPECT_EQ(actual.tm_wday, expected.tm_wday)
      << "tm_wday mismatch for " << context;
  EXPECT_EQ(actual.tm_yday, expected.tm_yday)
      << "tm_yday mismatch for " << context;
  EXPECT_EQ(actual.tm_isdst, expected.tm_isdst)
      << "tm_isdst mismatch for " << context;
}

}  // namespace nplb
