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

#include <initializer_list>
#include <list>

#include "starboard/common/ref_counted.h"
#include "starboard/shared/starboard/player/filter/video_frame_internal.h"
#include "starboard/shared/starboard/player/filter/video_frame_rate_estimator.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace testing {
namespace {

using Frames = VideoFrameRateEstimator::Frames;

auto kInvalidFrameRate = VideoFrameRateEstimator::kInvalidFrameRate;
constexpr double kFrameRateEpisilon = 0.05;

void SkipFrames(size_t frames_to_skip,
                Frames* frames,
                VideoFrameRateEstimator* estimator) {
  ASSERT_TRUE(frames);
  ASSERT_TRUE(estimator);
  ASSERT_LE(frames_to_skip, frames->size());

  while (frames_to_skip > 0) {
    --frames_to_skip;
    frames->pop_front();
    estimator->Update(*frames);
  }
}

Frames CreateFrames(const std::initializer_list<SbTime>& timestamps) {
  Frames frames;
  for (auto timestamp : timestamps) {
    frames.push_back(new VideoFrame(timestamp));
  }
  return frames;
}

TEST(VideoFrameRateEstimatorTest, Default) {
  VideoFrameRateEstimator estimator;
  EXPECT_EQ(estimator.frame_rate(), kInvalidFrameRate);

  estimator.Reset();
  EXPECT_EQ(estimator.frame_rate(), kInvalidFrameRate);
}

TEST(VideoFrameRateEstimatorTest, SingleFrame) {
  VideoFrameRateEstimator estimator;

  auto frames = CreateFrames({0});
  estimator.Update(frames);
  EXPECT_EQ(estimator.frame_rate(), kInvalidFrameRate);
}

TEST(VideoFrameRateEstimatorTest, TwoFrames) {
  VideoFrameRateEstimator estimator;

  auto frames = CreateFrames({0, 33333});
  estimator.Update(frames);
  ASSERT_NEAR(estimator.frame_rate(), 30, kFrameRateEpisilon);
}

TEST(VideoFrameRateEstimatorTest, Perfect30fps) {
  VideoFrameRateEstimator estimator;

  auto frames = CreateFrames({0, 33333, 66666, 100000, 133333, 166666, 200000});
  estimator.Update(frames);
  ASSERT_NEAR(estimator.frame_rate(), 30, kFrameRateEpisilon);
}

TEST(VideoFrameRateEstimatorTest, Imperfect30fps) {
  VideoFrameRateEstimator estimator;

  auto frames = CreateFrames({0, 34000, 67000, 100000, 133000, 167000, 200000});
  estimator.Update(frames);
  ASSERT_NEAR(estimator.frame_rate(), 30, kFrameRateEpisilon);
}

TEST(VideoFrameRateEstimatorTest, 60fps) {
  VideoFrameRateEstimator estimator;

  auto frames = CreateFrames({0, 16666, 33333, 50000, 66666, 83333, 100000});
  estimator.Update(frames);
  ASSERT_NEAR(estimator.frame_rate(), 60, kFrameRateEpisilon);
}

TEST(VideoFrameRateEstimatorTest, 60fpsStartedFromNonZero) {
  VideoFrameRateEstimator estimator;

  auto frames = CreateFrames({50000, 66666, 83333, 100000});
  estimator.Update(frames);
  ASSERT_NEAR(estimator.frame_rate(), 60, kFrameRateEpisilon);
}

TEST(VideoFrameRateEstimatorTest, 30fpsMultipleUpdates) {
  VideoFrameRateEstimator estimator;

  auto frames = CreateFrames({0, 33333, 66666, 100000, 133333, 166666, 200000});
  estimator.Update(frames);
  ASSERT_NEAR(estimator.frame_rate(), 30, kFrameRateEpisilon);

  SkipFrames(3, &frames, &estimator);
  ASSERT_NEAR(estimator.frame_rate(), 30, kFrameRateEpisilon);

  SkipFrames(4, &frames, &estimator);
  ASSERT_NEAR(estimator.frame_rate(), 30, kFrameRateEpisilon);

  ASSERT_TRUE(frames.empty());
  frames = CreateFrames({233333});
  estimator.Update(frames);
  ASSERT_NEAR(estimator.frame_rate(), 30, kFrameRateEpisilon);
  SkipFrames(1, &frames, &estimator);

  frames = CreateFrames({266666, 300000, 333333, 366666, 400000});
  estimator.Update(frames);
  ASSERT_NEAR(estimator.frame_rate(), 30, kFrameRateEpisilon);
}

TEST(VideoFrameRateEstimatorTest, 30fpsTo60fpsTransitionWithOneFrame) {
  VideoFrameRateEstimator estimator;

  auto frames = CreateFrames({0, 33333, 66666, 100000});
  estimator.Update(frames);
  ASSERT_NEAR(estimator.frame_rate(), 30, kFrameRateEpisilon);

  SkipFrames(3, &frames, &estimator);
  ASSERT_NEAR(estimator.frame_rate(), 30, kFrameRateEpisilon);

  SkipFrames(1, &frames, &estimator);

  frames = CreateFrames({116666});
  estimator.Update(frames);
  ASSERT_NEAR(estimator.frame_rate(), 60, kFrameRateEpisilon);
}

TEST(VideoFrameRateEstimatorTest, 30fpsTo60fpsTransitionWithReset) {
  VideoFrameRateEstimator estimator;

  auto frames = CreateFrames({0, 33333, 66666, 100000});
  estimator.Update(frames);
  ASSERT_NEAR(estimator.frame_rate(), 30, kFrameRateEpisilon);

  estimator.Reset();

  frames = CreateFrames({116666});
  estimator.Update(frames);
  EXPECT_EQ(estimator.frame_rate(), kInvalidFrameRate);

  frames = CreateFrames({116666, 133333});
  estimator.Update(frames);
  ASSERT_NEAR(estimator.frame_rate(), 60, kFrameRateEpisilon);
}

TEST(VideoFrameRateEstimatorTest, 30fpsTo60fpsTransitionWithMultipleFrames) {
  VideoFrameRateEstimator estimator;

  auto frames = CreateFrames({0, 33333, 66666, 100000, 116666, 133333, 150000});
  estimator.Update(frames);
  ASSERT_NEAR(estimator.frame_rate(), 30, kFrameRateEpisilon);

  SkipFrames(3, &frames, &estimator);
  ASSERT_NEAR(estimator.frame_rate(), 30, kFrameRateEpisilon);

  SkipFrames(1, &frames, &estimator);
  ASSERT_NEAR(estimator.frame_rate(), 60, kFrameRateEpisilon);

  SkipFrames(2, &frames, &estimator);
  ASSERT_NEAR(estimator.frame_rate(), 60, kFrameRateEpisilon);

  SkipFrames(1, &frames, &estimator);
  ASSERT_NEAR(estimator.frame_rate(), 60, kFrameRateEpisilon);
}

TEST(VideoFrameRateEstimatorTest, 30fpsTo60fpsTo30fps) {
  VideoFrameRateEstimator estimator;

  auto frames = CreateFrames({0, 33333, 66666, 100000, 116666, 150000});
  estimator.Update(frames);
  ASSERT_NEAR(estimator.frame_rate(), 30, kFrameRateEpisilon);

  SkipFrames(3, &frames, &estimator);
  ASSERT_NEAR(estimator.frame_rate(), 30, kFrameRateEpisilon);

  SkipFrames(1, &frames, &estimator);
  ASSERT_NEAR(estimator.frame_rate(), 60, kFrameRateEpisilon);

  SkipFrames(1, &frames, &estimator);
  ASSERT_NEAR(estimator.frame_rate(), 30, kFrameRateEpisilon);
}

TEST(VideoFrameRateEstimatorTest, EndOfStream) {
  VideoFrameRateEstimator estimator;

  auto frames = CreateFrames({0});
  frames.push_back(VideoFrame::CreateEOSFrame());
  estimator.Update(frames);
  ASSERT_EQ(estimator.frame_rate(), kInvalidFrameRate);

  frames = CreateFrames({0, 33333});
  frames.push_back(VideoFrame::CreateEOSFrame());
  estimator.Update(frames);
  ASSERT_NEAR(estimator.frame_rate(), 30, kFrameRateEpisilon);

  frames = CreateFrames({0, 33333, 66666, 100000, 116666, 150000});
  estimator.Update(frames);
  SkipFrames(5, &frames, &estimator);
  frames = CreateFrames({183333});
  frames.push_back(VideoFrame::CreateEOSFrame());
  estimator.Update(frames);
  SkipFrames(1, &frames, &estimator);
  estimator.Update(frames);
  ASSERT_NEAR(estimator.frame_rate(), 30, kFrameRateEpisilon);
}

}  // namespace
}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
