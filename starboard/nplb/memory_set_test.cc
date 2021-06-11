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

#include "starboard/memory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION < 13

namespace starboard {
namespace nplb {
namespace {

const size_t kSize = 1024 * 128;

TEST(SbMemorySetTest, SetsSomeData) {
  void* memory = SbMemoryAllocate(kSize);
  char* data = static_cast<char*>(memory);
  for (int i = 0; i < kSize; ++i) {
    data[i] = static_cast<char>(i);
  }

  void* result = SbMemorySet(memory, 0xCD, kSize);
  EXPECT_EQ(memory, result);
  for (int i = 0; i < kSize; ++i) {
    ASSERT_EQ('\xCD', data[i]);
  }

  SbMemoryDeallocate(memory);
}

TEST(SbMemorySetTest, SetsZeroData) {
  void* memory = SbMemoryAllocate(kSize);
  char* data = static_cast<char*>(memory);
  for (int i = 0; i < kSize; ++i) {
    data[i] = static_cast<char>(i);
  }

  void* result = SbMemorySet(memory, 0xCD, 0);
  EXPECT_EQ(memory, result);
  for (int i = 0; i < kSize; ++i) {
    ASSERT_EQ(static_cast<char>(i), data[i]);
  }

  SbMemoryDeallocate(memory);
}

TEST(SbMemorySetTest, IgnoresExtraData) {
  void* memory = SbMemoryAllocate(kSize);
  char* data = static_cast<char*>(memory);
  for (int i = 0; i < kSize; ++i) {
    data[i] = static_cast<char>(i);
  }

  void* result = SbMemorySet(memory, 0x6789ABCD, kSize);
  EXPECT_EQ(memory, result);
  for (int i = 0; i < kSize; ++i) {
    ASSERT_EQ('\xCD', data[i]);
  }

  SbMemoryDeallocate(memory);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif // SB_API_VERSION < 13
