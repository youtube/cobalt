// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/video_frame_cadence_pattern_generator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace testing {
namespace {

static const int kTimesToIterate = 20000;

TEST(VideoFrameCadencePatternGeneratorTest, 120fpsFrameRateOn60fpsRefreshRate) {
  VideoFrameCadencePatternGenerator generator;

  generator.UpdateRefreshRateAndMaybeReset(60);
  generator.UpdateFrameRate(120);
  for (int i = 0; i < kTimesToIterate; ++i) {
    ASSERT_EQ(generator.GetNumberOfTimesCurrentFrameDisplays(), 1);
    generator.AdvanceToNextFrame();
    ASSERT_EQ(generator.GetNumberOfTimesCurrentFrameDisplays(), 0);
    generator.AdvanceToNextFrame();
  }
}

TEST(VideoFrameCadencePatternGeneratorTest, 60fpsFrameRateOn60fpsRefreshRate) {
  VideoFrameCadencePatternGenerator generator;

  generator.UpdateRefreshRateAndMaybeReset(60);
  generator.UpdateFrameRate(60);
  for (int i = 0; i < kTimesToIterate; ++i) {
    ASSERT_EQ(generator.GetNumberOfTimesCurrentFrameDisplays(), 1);
    generator.AdvanceToNextFrame();
  }
}

TEST(VideoFrameCadencePatternGeneratorTest, 30fpsFrameRateOn60fpsRefreshRate) {
  VideoFrameCadencePatternGenerator generator;

  generator.UpdateRefreshRateAndMaybeReset(60);
  generator.UpdateFrameRate(30);
  for (int i = 0; i < kTimesToIterate; ++i) {
    ASSERT_EQ(generator.GetNumberOfTimesCurrentFrameDisplays(), 2);
    generator.AdvanceToNextFrame();
  }
}

TEST(VideoFrameCadencePatternGeneratorTest, 30fpsFrameRateOn30fpsRefreshRate) {
  VideoFrameCadencePatternGenerator generator;

  generator.UpdateRefreshRateAndMaybeReset(30);
  generator.UpdateFrameRate(30);
  for (int i = 0; i < kTimesToIterate; ++i) {
    ASSERT_EQ(generator.GetNumberOfTimesCurrentFrameDisplays(), 1);
    generator.AdvanceToNextFrame();
  }
}

TEST(VideoFrameCadencePatternGeneratorTest, 25fpsFrameRateOn60fpsRefreshRate) {
  VideoFrameCadencePatternGenerator generator;

  generator.UpdateRefreshRateAndMaybeReset(60);
  generator.UpdateFrameRate(25);
  for (int i = 0; i < kTimesToIterate; ++i) {
    ASSERT_EQ(generator.GetNumberOfTimesCurrentFrameDisplays(), 3);
    generator.AdvanceToNextFrame();
    ASSERT_EQ(generator.GetNumberOfTimesCurrentFrameDisplays(), 2);
    generator.AdvanceToNextFrame();
    ASSERT_EQ(generator.GetNumberOfTimesCurrentFrameDisplays(), 3);
    generator.AdvanceToNextFrame();
    ASSERT_EQ(generator.GetNumberOfTimesCurrentFrameDisplays(), 2);
    generator.AdvanceToNextFrame();
    ASSERT_EQ(generator.GetNumberOfTimesCurrentFrameDisplays(), 2);
    generator.AdvanceToNextFrame();
  }
}

TEST(VideoFrameCadencePatternGeneratorTest, 24fpsFrameRateOn60fpsRefreshRate) {
  VideoFrameCadencePatternGenerator generator;

  generator.UpdateRefreshRateAndMaybeReset(60);
  generator.UpdateFrameRate(24);
  for (int i = 0; i < kTimesToIterate; ++i) {
    ASSERT_EQ(generator.GetNumberOfTimesCurrentFrameDisplays(), 3);
    generator.AdvanceToNextFrame();
    ASSERT_EQ(generator.GetNumberOfTimesCurrentFrameDisplays(), 2);
    generator.AdvanceToNextFrame();
  }
}

}  // namespace
}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
