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

TEST(SbMemoryMoveTest, MovesSomeData) {
  void* memory1 = SbMemoryAllocate(kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory1);
  void* memory2 = SbMemoryAllocate(kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory2);
  char* data1 = static_cast<char*>(memory1);
  char* data2 = static_cast<char*>(memory2);
  for (int i = 0; i < kSize; ++i) {
    data1[i] = i;
    data2[i] = 0;
  }

  void* result = SbMemoryMove(memory2, memory1, kSize);
  EXPECT_EQ(memory2, result);

  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(data1[i], static_cast<char>(i));
    EXPECT_EQ(data2[i], static_cast<char>(i));
  }

  SbMemoryDeallocate(memory1);
  SbMemoryDeallocate(memory2);
}

TEST(SbMemoryMoveTest, MovesZeroData) {
  void* memory1 = SbMemoryAllocate(kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory1);
  void* memory2 = SbMemoryAllocate(kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory2);
  char* data1 = static_cast<char*>(memory1);
  char* data2 = static_cast<char*>(memory2);
  for (int i = 0; i < kSize; ++i) {
    data1[i] = static_cast<char>(i);
    data2[i] = static_cast<char>(i + 1);
  }

  void* result = SbMemoryMove(memory2, memory1, 0);
  EXPECT_EQ(memory2, result);

  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(data1[i], static_cast<char>(i));
    EXPECT_EQ(data2[i], static_cast<char>(i + 1));
  }

  SbMemoryDeallocate(memory1);
  SbMemoryDeallocate(memory2);
}

TEST(SbMemoryMoveTest, MovesOverlappingDataForward) {
  void* memory = SbMemoryAllocate(kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory);
  char* data = static_cast<char*>(memory);
  for (int i = 0; i < kSize; ++i) {
    data[i] = static_cast<char>(i);
  }

  void* result = SbMemoryMove(data + 3, data, kSize - 3);
  EXPECT_EQ(data + 3, result);

  for (int i = 0; i < kSize - 3; ++i) {
    EXPECT_EQ(data[i + 3], static_cast<char>(i));
  }

  SbMemoryDeallocate(memory);
}

TEST(SbMemoryMoveTest, MovesOverlappingDataBackwards) {
  void* memory = SbMemoryAllocate(kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory);
  char* data = static_cast<char*>(memory);
  for (int i = 0; i < kSize; ++i) {
    data[i] = static_cast<char>(i);
  }

  void* result = SbMemoryMove(data, data + 3, kSize - 3);
  EXPECT_EQ(memory, result);

  for (int i = 0; i < kSize - 3; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i + 3));
  }

  SbMemoryDeallocate(memory);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif