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

/*
  The following strdup error conditions are not tested due to the difficulty of
  reliably triggering them in a portable unit testing environment:

  - ENOMEM: Insufficient memory is available to fulfill the request. Forcing
    the system to run out of memory is not feasible in a unit test.
*/

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard::nplb {
namespace {

TEST(PosixStringDuplicateTest, SunnyDayBasic) {
  const char* original = "Hello, World!";
  char* duplicated = strdup(original);

  ASSERT_NE(duplicated, nullptr);
  EXPECT_STREQ(original, duplicated);
  EXPECT_NE(static_cast<const void*>(original),
            static_cast<const void*>(duplicated));

  free(duplicated);
}

TEST(PosixStringDuplicateTest, SunnyDayEmptyString) {
  const char* original = "";
  char* duplicated = strdup(original);

  ASSERT_NE(duplicated, nullptr);
  EXPECT_STREQ(original, duplicated);
  EXPECT_NE(static_cast<const void*>(original),
            static_cast<const void*>(duplicated));

  free(duplicated);
}

TEST(PosixStringDuplicateTest, SunnyDayLongString) {
  std::string long_string(1024, 'a');
  const char* original = long_string.c_str();
  char* duplicated = strdup(original);

  ASSERT_NE(duplicated, nullptr);
  EXPECT_STREQ(original, duplicated);
  EXPECT_NE(static_cast<const void*>(original),
            static_cast<const void*>(duplicated));

  free(duplicated);
}

TEST(PosixStringDuplicateTest, SunnyDayStringWithSpaces) {
  const char* original = "  leading and trailing spaces  ";
  char* duplicated = strdup(original);

  ASSERT_NE(duplicated, nullptr);
  EXPECT_STREQ(original, duplicated);
  EXPECT_NE(static_cast<const void*>(original),
            static_cast<const void*>(duplicated));

  free(duplicated);
}

}  // namespace
}  // namespace starboard::nplb
