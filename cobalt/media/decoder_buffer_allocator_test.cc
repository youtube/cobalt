// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/decoder_buffer_allocator.h"

#include "starboard/media.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace media {
namespace {

class DecoderBufferAllocatorTest : public testing::Test {
 protected:
  DecoderBufferAllocatorTest()
      : allocator_(DecoderBufferAllocator()),
        codec_(kSbMediaVideoCodecVp9),
        width_(1920),
        height_(1080),
        bit_depth_(8),
        initial_buffer_budget_(allocator_.GetVideoBufferBudget(
            codec_, width_, height_, bit_depth_)) {}
  ~DecoderBufferAllocatorTest() = default;

  int GetInitialVideoBufferBudget() const { return initial_buffer_budget_; }

  int GetCurrentVideoBufferBudget() const {
    return allocator_.GetVideoBufferBudget(codec_, width_, height_, bit_depth_);
  }

  bool SetVideoBufferBudget(const int video_buffer_budget) {
    return allocator_.SetVideoBufferBudget(video_buffer_budget);
  }

 private:
  DecoderBufferAllocator allocator_;
  const SbMediaVideoCodec codec_;
  const int width_;
  const int height_;
  const int bit_depth_;
  const int initial_buffer_budget_;
};

TEST_F(DecoderBufferAllocatorTest, SetVideoBufferBudget) {
  int buffer_budget = 123'456'789;  // ~123 MB
  ASSERT_TRUE(SetVideoBufferBudget(buffer_budget));
  EXPECT_EQ(GetCurrentVideoBufferBudget(), buffer_budget);

  buffer_budget = 1'234'567;  // ~1 MB
  ASSERT_TRUE(SetVideoBufferBudget(buffer_budget));
  EXPECT_EQ(GetCurrentVideoBufferBudget(), buffer_budget);
}

TEST_F(DecoderBufferAllocatorTest, SetAndResetVideoBufferBudget) {
  int buffer_budget = 123'456'789;  // ~123 MB
  ASSERT_TRUE(SetVideoBufferBudget(buffer_budget));
  EXPECT_EQ(GetCurrentVideoBufferBudget(), buffer_budget);

  buffer_budget = 0;
  ASSERT_TRUE(SetVideoBufferBudget(buffer_budget));
  EXPECT_EQ(GetCurrentVideoBufferBudget(), GetInitialVideoBufferBudget());
}

TEST_F(DecoderBufferAllocatorTest, SetVideoBufferBudgetInvalid) {
  int buffer_budget = -123'456'789;
  ASSERT_FALSE(SetVideoBufferBudget(buffer_budget));
  EXPECT_EQ(GetCurrentVideoBufferBudget(), GetInitialVideoBufferBudget());

  // Test setting to invalid budget after setting to valid budget.
  buffer_budget = 1'234'567;  // ~1 MB
  ASSERT_TRUE(SetVideoBufferBudget(buffer_budget));
  ASSERT_EQ(GetCurrentVideoBufferBudget(), buffer_budget);

  int current_buffer_budget = GetCurrentVideoBufferBudget();
  buffer_budget = -1'000'000'000;
  ASSERT_FALSE(SetVideoBufferBudget(buffer_budget));
  EXPECT_EQ(GetCurrentVideoBufferBudget(), current_buffer_budget);
}

}  // namespace
}  // namespace media
}  // namespace cobalt
