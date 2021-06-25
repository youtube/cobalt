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

#include <cwchar>

#include "starboard/common/string.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION < 13

namespace {

const wchar_t kSource[] = L"0123456789";
const wchar_t kInitial[] = L"ABCDEFGHIJ";

void TestConcat(const wchar_t* initial, const wchar_t* source, bool is_short) {
  const int kDestinationOffset = 16;
  int initial_length = static_cast<int>(wcslen(initial));
  int source_length = static_cast<int>(wcslen(source));
  int destination_size =
      initial_length + source_length + kDestinationOffset * 2;
  int destination_limit = initial_length + source_length + 1;
  if (is_short) {
    ASSERT_LT(1, destination_limit);
    destination_limit -= 1;
  }
  ASSERT_GT(1024, destination_size);
  wchar_t destination[1024] = {0};
  wchar_t* dest = destination + kDestinationOffset;
  SbStringCopyWide(dest, initial, destination_limit);
  int result = SbStringConcatWide(dest, source, destination_limit);

  EXPECT_EQ(initial_length + source_length, result);

  // Expected to be one less than the limit due to the null terminator.
  int expected_length = destination_limit - 1;
  EXPECT_EQ(expected_length, wcslen(dest));

  // Validate the memory before the destination isn't touched.
  for (int i = 0; i < kDestinationOffset; ++i) {
    EXPECT_EQ(0, destination[i]);
  }

  // Validate the previous data wasn't modified.
  for (int i = 0; i < initial_length; ++i) {
    EXPECT_EQ(initial[i], dest[i]);
  }

  // Validate the new data was copied.
  for (int i = 0; i < source_length - (is_short ? 1 : 0); ++i) {
    EXPECT_EQ(source[i], dest[i + initial_length]);
  }

  // Validate the memory after the copy isn't touched.
  for (int i = 0; i < kDestinationOffset; ++i) {
    EXPECT_EQ(0, dest[expected_length + i]);
  }
}

TEST(SbStringConcatWideTest, SunnyDay) {
  TestConcat(kInitial, kSource, false);
  TestConcat(L"", kSource, false);
}

TEST(SbStringConcatWideTest, SunnyDayEmpty) {
  TestConcat(kInitial, L"", false);
  TestConcat(L"", L"", false);
}

TEST(SbStringConcatWideTest, SunnyDayShort) {
  TestConcat(kInitial, kSource, true);
  TestConcat(L"", kSource, true);
}

}  // namespace

#endif  // SB_API_VERSION < 13
