// Copyright 2016 Google Inc. All Rights Reserved.
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

#if SB_HAS(MMAP)
const size_t kSize = SB_MEMORY_PAGE_SIZE * 8;
const void* kFailed = SB_MEMORY_MAP_FAILED;

TEST(SbMemoryMapTest, AllocatesNormally) {
  void* memory = SbMemoryMap(kSize, "test");
  EXPECT_NE(kFailed, memory);
  EXPECT_TRUE(SbMemoryUnmap(memory, kSize));
}

TEST(SbMemoryMapTest, AllocatesZero) {
  void* memory = SbMemoryMap(0, "test");
  EXPECT_EQ(kFailed, memory);
  EXPECT_FALSE(SbMemoryUnmap(memory, 0));
}

TEST(SbMemoryMapTest, AllocatesOne) {
  void* memory = SbMemoryMap(1, "test");
  EXPECT_NE(kFailed, memory);
  EXPECT_TRUE(SbMemoryUnmap(memory, 1));
}

TEST(SbMemoryMapTest, AllocatesOnePage) {
  void* memory = SbMemoryMap(SB_MEMORY_PAGE_SIZE, "test");
  EXPECT_NE(kFailed, memory);
  EXPECT_TRUE(SbMemoryUnmap(memory, SB_MEMORY_PAGE_SIZE));
}

TEST(SbMemoryMapTest, CanReadWriteToResult) {
  void* memory = SbMemoryMap(kSize, "test");
  ASSERT_NE(kFailed, memory);
  char* data = static_cast<char*>(memory);
  for (int i = 0; i < kSize; ++i) {
    data[i] = static_cast<char>(i);
  }

  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  EXPECT_TRUE(SbMemoryUnmap(memory, kSize));
}
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
