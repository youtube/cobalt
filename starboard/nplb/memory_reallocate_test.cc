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

namespace starboard {
namespace nplb {
namespace {

const size_t kSize = 1024 * 128;

TEST(SbMemoryReallocateTest, AllocatesNormally) {
  void* memory = SbMemoryReallocate(NULL, kSize);
  EXPECT_NE(static_cast<void*>(NULL), memory);
  SbMemoryDeallocate(memory);
}

TEST(SbMemoryReallocateTest, AllocatesZero) {
  void* memory = SbMemoryReallocate(NULL, 0);
  // We can't expect anything here because some implementations may return an
  // allocated zero-size memory block, and some implementations may return NULL.
  SbMemoryDeallocate(memory);
}

TEST(SbMemoryReallocateTest, AllocatesOne) {
  void* memory = SbMemoryReallocate(NULL, 1);
  EXPECT_NE(static_cast<void*>(NULL), memory);
  SbMemoryDeallocate(memory);
}

TEST(SbMemoryReallocateTest, CanReadWriteToResult) {
  void* memory = SbMemoryReallocate(NULL, kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory);
  char* data = static_cast<char*>(memory);
  for (int i = 0; i < kSize; ++i) {
    data[i] = i;
  }

  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  SbMemoryDeallocate(memory);
}

TEST(SbMemoryReallocateTest, ReallocatesSmaller) {
  void* memory = SbMemoryAllocate(kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory);
  char* data = static_cast<char*>(memory);
  for (int i = 0; i < kSize; ++i) {
    data[i] = i;
  }

  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  memory = SbMemoryReallocate(memory, kSize / 2);
  data = static_cast<char*>(memory);
  ASSERT_NE(static_cast<void*>(NULL), memory);
  for (int i = 0; i < kSize / 2; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  SbMemoryDeallocate(memory);
}

TEST(SbMemoryReallocateTest, ReallocatesBigger) {
  void* memory = SbMemoryAllocate(kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory);
  char* data = static_cast<char*>(memory);
  for (int i = 0; i < kSize; ++i) {
    data[i] = i;
  }

  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  memory = SbMemoryReallocate(memory, kSize * 2);
  ASSERT_NE(static_cast<void*>(NULL), memory);
  data = static_cast<char*>(memory);
  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  for (int i = kSize; i < kSize * 2; ++i) {
    data[i] = i;
  }

  for (int i = kSize; i < kSize * 2; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  SbMemoryDeallocate(memory);
}

TEST(SbMemoryReallocateTest, ReallocatestoZero) {
  void* memory = SbMemoryAllocate(kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory);
  memory = SbMemoryReallocate(memory, 0);
  // See allocates zero above.
  SbMemoryDeallocate(memory);
}

TEST(SbMemoryReallocateTest, ReallocatestoSameSize) {
  void* memory = SbMemoryAllocate(kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory);
  char* data = static_cast<char*>(memory);
  for (int i = 0; i < kSize; ++i) {
    data[i] = i;
  }

  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  memory = SbMemoryReallocate(memory, kSize);
  data = static_cast<char*>(memory);
  ASSERT_NE(static_cast<void*>(NULL), memory);
  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  SbMemoryDeallocate(memory);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
