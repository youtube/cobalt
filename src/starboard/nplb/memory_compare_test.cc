// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "starboard/memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const size_t kSize = 1024 * 128;

TEST(SbMemoryCompareTest, SunnyDayEquals) {
  void* memory1 = SbMemoryAllocate(kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory1);
  void* memory2 = SbMemoryAllocate(kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory2);
  char* data1 = static_cast<char*>(memory1);
  char* data2 = static_cast<char*>(memory2);
  for (int i = 0; i < kSize; ++i) {
    data1[i] = static_cast<char>(i);
    data2[i] = static_cast<char>(i);
  }

  int result = SbMemoryCompare(memory1, memory2, kSize);
  EXPECT_EQ(0, result);

  SbMemoryDeallocate(memory1);
  SbMemoryDeallocate(memory2);
}

TEST(SbMemoryCompareTest, SunnyDayLessAndMore) {
  void* memory1 = SbMemoryAllocate(kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory1);
  void* memory2 = SbMemoryAllocate(kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory2);
  char* data1 = static_cast<char*>(memory1);
  char* data2 = static_cast<char*>(memory2);
  for (int i = 0; i < kSize; ++i) {
    data1[i] = static_cast<char>(i);
    char x = data1[i];
    if (x > 0) {
      --x;
    }
    data2[i] = x;
  }

  int result = SbMemoryCompare(memory1, memory2, kSize);
  EXPECT_LT(0, result);

  result = SbMemoryCompare(memory2, memory1, kSize);
  EXPECT_GT(0, result);

  SbMemoryDeallocate(memory1);
  SbMemoryDeallocate(memory2);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
