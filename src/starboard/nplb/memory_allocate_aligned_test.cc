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

#include "starboard/common/memory.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Choose a size that isn't naturally aligned to anything.
const size_t kSize = 1024 * 128 + 1;

TEST(SbMemoryAllocateAlignedTest, AllocatesAligned) {
  const size_t kMaxAlign = 4096 + 1;
  for (size_t align = 2; align < kMaxAlign; align <<= 1) {
    void* memory = SbMemoryAllocateAligned(align, kSize);
    ASSERT_NE(static_cast<void*>(NULL), memory);
    EXPECT_TRUE(starboard::common::MemoryIsAligned(memory, align));
    SbMemoryDeallocateAligned(memory);
  }
}

TEST(SbMemoryAllocateAlignedTest, AllocatesAlignedZero) {
  const size_t kMaxAlign = 4096 + 1;
  for (size_t align = 2; align < kMaxAlign; align <<= 1) {
    void* memory = SbMemoryAllocateAligned(align, 0);
    // We can't expect anything here because some implementations may return an
    // allocated zero-size memory block, and some implementations may return
    // NULL.
    if (memory) {
      EXPECT_TRUE(starboard::common::MemoryIsAligned(memory, align));
    }
    SbMemoryDeallocateAligned(memory);
  }
}

TEST(SbMemoryAllocateAlignedTest, CanReadWriteToResult) {
  const size_t kAlign = 4096;
  void* memory = SbMemoryAllocateAligned(kAlign, kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory);
  char* data = static_cast<char*>(memory);
  for (int i = 0; i < kSize; ++i) {
    data[i] = static_cast<char>(i);
  }

  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  SbMemoryDeallocateAligned(memory);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
