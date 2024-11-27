// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace common {
namespace {

TEST(MemoryIsAlignedTest, CheckAlignmentVariousSizes) {
  uint64_t dummy = {0};  // 64-bit aligned
  uint8_t* ptr = reinterpret_cast<uint8_t*>(dummy);
  EXPECT_TRUE(MemoryIsAligned(ptr, 8));
  EXPECT_TRUE(MemoryIsAligned(ptr + 4, 4));
  EXPECT_FALSE(MemoryIsAligned(ptr + 4, 8));
}

TEST(MemoryAlignToPageSizeTest, AlignsVariousSizes) {
  EXPECT_EQ(0, MemoryAlignToPageSize(0));
  EXPECT_EQ(kSbMemoryPageSize, MemoryAlignToPageSize(1));
  EXPECT_EQ(kSbMemoryPageSize, MemoryAlignToPageSize(kSbMemoryPageSize - 1));
  EXPECT_EQ(kSbMemoryPageSize, MemoryAlignToPageSize(kSbMemoryPageSize));
  EXPECT_EQ(100 * kSbMemoryPageSize,
            MemoryAlignToPageSize(100 * kSbMemoryPageSize));
}

}  // namespace
}  // namespace common
}  // namespace starboard
