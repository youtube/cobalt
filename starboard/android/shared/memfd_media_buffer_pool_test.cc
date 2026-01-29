// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/memfd_media_buffer_pool.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace android {
namespace shared {
namespace {

TEST(MemFdMediaBufferPoolTest, BasicOperations) {
  MemFdMediaBufferPool* pool = MemFdMediaBufferPool::Get();

  ASSERT_TRUE(pool);

  // Ensure it's in a clean state.
  pool->ShrinkToZero();

  constexpr size_t kSize1 = 1024;
  EXPECT_TRUE(pool->ExpandTo(kSize1));

  constexpr char kData1[] = "Hello, MemFd!";
  constexpr size_t kDataSize1 = sizeof(kData1);
  pool->Write(0, kData1, kDataSize1);

  char buffer1[kDataSize1];
  pool->Read(0, buffer1, kDataSize1);
  EXPECT_STREQ(kData1, buffer1);

  // Expand further.
  constexpr size_t kSize2 = 2048;
  EXPECT_TRUE(pool->ExpandTo(kSize2));

  constexpr char kData2[] = "Another piece of data at offset 1024";
  constexpr size_t kDataSize2 = sizeof(kData2);
  pool->Write(1024, kData2, kDataSize2);

  char buffer2[kDataSize2];
  pool->Read(1024, buffer2, kDataSize2);
  EXPECT_STREQ(kData2, buffer2);

  // Verify first data is still there.
  pool->Read(0, buffer1, kDataSize1);
  EXPECT_STREQ(kData1, buffer1);

  // Shrink to zero.
  pool->ShrinkToZero();

  // Expand again and verify.
  EXPECT_TRUE(pool->ExpandTo(kSize1));
  constexpr char kData3[] = "New data after shrink";
  constexpr size_t kDataSize3 = sizeof(kData3);
  pool->Write(0, kData3, kDataSize3);
  char buffer3[kDataSize3];
  pool->Read(0, buffer3, kDataSize3);
  EXPECT_STREQ(kData3, buffer3);

  // Clean up.
  pool->ShrinkToZero();
}

TEST(MemFdMediaBufferPoolTest, RepeatedExpand) {
  MemFdMediaBufferPool* pool = MemFdMediaBufferPool::Get();

  ASSERT_TRUE(pool);

  EXPECT_TRUE(pool->ExpandTo(1024));
  // Expanding to a smaller or equal size should return true and do nothing.
  EXPECT_TRUE(pool->ExpandTo(512));
  EXPECT_TRUE(pool->ExpandTo(1024));

  pool->ShrinkToZero();
}

TEST(MemFdMediaBufferPoolTest, Fail) {
  // Just trying to see if the trybots can pick up this failure.
  ASSERT_TRUE(false);
}

}  // namespace
}  // namespace shared
}  // namespace android
}  // namespace starboard
