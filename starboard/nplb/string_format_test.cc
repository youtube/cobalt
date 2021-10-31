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

// Here we are not trying to do anything fancy, just to really sanity check that
// this is hooked up to something.

#include "starboard/common/string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

int Format(char* out_buffer, size_t buffer_size, const char* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = SbStringFormat(out_buffer, buffer_size, format, arguments);
  va_end(arguments);
  return result;
}

TEST(SbStringFormatTest, SunnyDay) {
  const char kExpected[] = "a1b2c3test";
  char destination[1024] = {0};
  int result = Format(destination, SB_ARRAY_SIZE(destination), "a%db%dc%d%s", 1,
                      2, 3, "test");
  size_t expected_length = strlen(kExpected);
  EXPECT_EQ(expected_length, result);
  for (size_t i = 0; i <= expected_length; ++i) {
    EXPECT_EQ(kExpected[i], destination[i]);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
