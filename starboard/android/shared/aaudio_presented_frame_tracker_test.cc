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

#include "starboard/android/shared/aaudio_presented_frame_tracker.h"

#include <cstdint>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

constexpr int kSampleRate = 48'000;
// An arbitrary current time, in microseconds.
constexpr int64_t kNowUs = 1'000'000'000;
// The number of microseconds it takes to present 48 frames at |kSampleRate|.
constexpr int64_t k48FramesUs = 1'000;

class AaudioPresentedFrameTrackerTest : public ::testing::Test {
 protected:
  // Reports a fresh timestamp, i.e. taken at |kNowUs|.
  int OnFreshTimestamp(int64_t timestamp_position, int64_t written_position) {
    return tracker_.OnTimestamp(timestamp_position, kNowUs, kNowUs,
                                written_position);
  }

  AaudioPresentedFrameTracker tracker_{kSampleRate};
};

TEST_F(AaudioPresentedFrameTrackerTest, InitiallyEmpty) {
  EXPECT_EQ(tracker_.frames_in_flight(), 0);
  EXPECT_EQ(OnFreshTimestamp(1'000, 1'000), 0);
  EXPECT_EQ(tracker_.frames_in_flight(), 0);
}

TEST_F(AaudioPresentedFrameTrackerTest, IgnoresNonPositiveFrameCounts) {
  tracker_.OnFramesCopied(0, 0);
  tracker_.OnFramesCopied(0, -10);
  EXPECT_EQ(tracker_.frames_in_flight(), 0);
  EXPECT_EQ(OnFreshTimestamp(100, 100), 0);
}

TEST_F(AaudioPresentedFrameTrackerTest,
       NothingPresentedBeforeTimestampReachesFirstSegment) {
  // Frames copied at position 960, e.g. after silence callbacks.
  tracker_.OnFramesCopied(960, 480);
  EXPECT_EQ(tracker_.frames_in_flight(), 480);

  EXPECT_EQ(OnFreshTimestamp(0, 1'440), 0);
  EXPECT_EQ(OnFreshTimestamp(480, 1'440), 0);
  // The timestamp position is the position of the next frame to be
  // presented, so the frame at 960 isn't presented yet.
  EXPECT_EQ(OnFreshTimestamp(960, 1'440), 0);
  EXPECT_EQ(tracker_.frames_in_flight(), 480);

  EXPECT_EQ(OnFreshTimestamp(961, 1'440), 1);
  EXPECT_EQ(tracker_.frames_in_flight(), 479);
}

TEST_F(AaudioPresentedFrameTrackerTest, PartialSegmentPresentation) {
  tracker_.OnFramesCopied(0, 480);

  EXPECT_EQ(OnFreshTimestamp(100, 480), 100);
  EXPECT_EQ(tracker_.frames_in_flight(), 380);
  EXPECT_EQ(OnFreshTimestamp(300, 480), 200);
  EXPECT_EQ(tracker_.frames_in_flight(), 180);
  EXPECT_EQ(OnFreshTimestamp(480, 480), 180);
  EXPECT_EQ(tracker_.frames_in_flight(), 0);
  // Nothing is reported twice.
  EXPECT_EQ(OnFreshTimestamp(480, 480), 0);
}

TEST_F(AaudioPresentedFrameTrackerTest,
       TimestampGoingBackwardsPresentsNothing) {
  tracker_.OnFramesCopied(0, 480);
  EXPECT_EQ(OnFreshTimestamp(200, 480), 200);
  EXPECT_EQ(OnFreshTimestamp(100, 480), 0);
  EXPECT_EQ(tracker_.frames_in_flight(), 280);
}

TEST_F(AaudioPresentedFrameTrackerTest, StaleTimestampIsExtrapolated) {
  tracker_.OnFramesCopied(0, 4'800);

  // The frame at 0 was presented 10 ms ago, so 480 frames are presented now.
  EXPECT_EQ(tracker_.OnTimestamp(0, kNowUs - 10 * k48FramesUs, kNowUs, 4'800),
            480);
  EXPECT_EQ(tracker_.frames_in_flight(), 4'320);

  // The frame at 480 was presented 5 ms ago.
  EXPECT_EQ(tracker_.OnTimestamp(480, kNowUs - 5 * k48FramesUs, kNowUs, 4'800),
            240);
  EXPECT_EQ(tracker_.frames_in_flight(), 4'080);
}

TEST_F(AaudioPresentedFrameTrackerTest, ExtrapolationRoundsDownPartialFrames) {
  tracker_.OnFramesCopied(0, 480);
  // 30 us at 48 kHz is 1.44 frames.
  EXPECT_EQ(tracker_.OnTimestamp(0, kNowUs - 30, kNowUs, 480), 1);
}

TEST_F(AaudioPresentedFrameTrackerTest,
       ExtrapolationIsCappedAtWrittenPosition) {
  tracker_.OnFramesCopied(0, 480);

  // A timestamp from 1 s ago extrapolates to 48'000 frames presented, but
  // frames after the written position can't have been presented.
  EXPECT_EQ(tracker_.OnTimestamp(0, kNowUs - 1'000'000, kNowUs, 240), 240);
  EXPECT_EQ(tracker_.frames_in_flight(), 240);

  // Also when the timestamp itself is past the written position.
  EXPECT_EQ(OnFreshTimestamp(1'000, 360), 120);
  EXPECT_EQ(tracker_.frames_in_flight(), 120);
}

TEST_F(AaudioPresentedFrameTrackerTest, FutureTimestampIsNotExtrapolated) {
  tracker_.OnFramesCopied(0, 480);
  // A timestamp later than now (the age is clamped to 0) must not move the
  // presented position backwards.
  EXPECT_EQ(tracker_.OnTimestamp(100, kNowUs + 10 * k48FramesUs, kNowUs, 480),
            100);
  EXPECT_EQ(tracker_.frames_in_flight(), 380);
}

// Contiguous copies are merged into one segment internally, which must not
// affect the frames presented.
TEST_F(AaudioPresentedFrameTrackerTest, PresentsAcrossContiguousCopies) {
  tracker_.OnFramesCopied(0, 192);
  tracker_.OnFramesCopied(192, 192);
  tracker_.OnFramesCopied(384, 96);
  EXPECT_EQ(tracker_.frames_in_flight(), 480);

  // Presentation crosses the boundaries of the original copies.
  EXPECT_EQ(OnFreshTimestamp(200, 480), 200);
  EXPECT_EQ(OnFreshTimestamp(400, 480), 200);
  EXPECT_EQ(OnFreshTimestamp(480, 480), 80);
  EXPECT_EQ(tracker_.frames_in_flight(), 0);
}

TEST_F(AaudioPresentedFrameTrackerTest, SkipsGapsBetweenSegments) {
  // Frames copied at [0, 192) and [384, 576); the callback in between output
  // silence, e.g. on underrun or while paused.
  tracker_.OnFramesCopied(0, 192);
  tracker_.OnFramesCopied(384, 192);
  EXPECT_EQ(tracker_.frames_in_flight(), 384);

  // Silence isn't reported as consumed.
  EXPECT_EQ(OnFreshTimestamp(300, 576), 192);
  EXPECT_EQ(tracker_.frames_in_flight(), 192);
  EXPECT_EQ(OnFreshTimestamp(384, 576), 0);
  EXPECT_EQ(OnFreshTimestamp(400, 576), 16);
  EXPECT_EQ(tracker_.frames_in_flight(), 176);
}

TEST_F(AaudioPresentedFrameTrackerTest, PresentsAcrossSeveralSegmentsAtOnce) {
  tracker_.OnFramesCopied(0, 100);
  tracker_.OnFramesCopied(200, 100);
  tracker_.OnFramesCopied(400, 100);

  EXPECT_EQ(OnFreshTimestamp(450, 500), 250);
  EXPECT_EQ(tracker_.frames_in_flight(), 50);
  EXPECT_EQ(OnFreshTimestamp(10'000, 500), 50);
  EXPECT_EQ(tracker_.frames_in_flight(), 0);
}

TEST_F(AaudioPresentedFrameTrackerTest, ResetDropsFramesInFlight) {
  tracker_.OnFramesCopied(0, 480);
  tracker_.OnFramesCopied(960, 480);
  EXPECT_EQ(OnFreshTimestamp(100, 1'440), 100);

  tracker_.Reset();
  EXPECT_EQ(tracker_.frames_in_flight(), 0);
  EXPECT_EQ(OnFreshTimestamp(1'440, 1'440), 0);

  // Positions may restart at 0 after Reset(), e.g. on a reopened stream.
  tracker_.OnFramesCopied(0, 240);
  EXPECT_EQ(tracker_.frames_in_flight(), 240);
  EXPECT_EQ(OnFreshTimestamp(240, 240), 240);
  EXPECT_EQ(tracker_.frames_in_flight(), 0);
}

TEST_F(AaudioPresentedFrameTrackerTest, FramesInFlightAccounting) {
  int64_t position = 0;
  int64_t presented_position = 0;
  int64_t total_copied = 0;
  int64_t total_presented = 0;
  // Simulate callbacks of 192 frames, every third one outputting silence,
  // with presentation lagging 2 callbacks behind.
  for (int i = 0; i < 100; ++i) {
    if (i % 3 != 2) {
      tracker_.OnFramesCopied(position, 192);
      total_copied += 192;
    }
    position += 192;
    presented_position = position - 2 * 192;
    total_presented += OnFreshTimestamp(presented_position, position);
    EXPECT_EQ(tracker_.frames_in_flight(), total_copied - total_presented);
  }
  EXPECT_LE(tracker_.frames_in_flight(), 2 * 192);

  total_presented += OnFreshTimestamp(position, position);
  EXPECT_EQ(total_presented, total_copied);
  EXPECT_EQ(tracker_.frames_in_flight(), 0);
}

TEST_F(AaudioPresentedFrameTrackerTest, LargePositions) {
  // Positions beyond the range of int32_t, i.e. after ~12 hours at 48 kHz.
  constexpr int64_t kStart = (int64_t{1} << 40) + 7;
  tracker_.OnFramesCopied(kStart, 480);
  tracker_.OnFramesCopied(kStart + 480, 480);
  EXPECT_EQ(tracker_.frames_in_flight(), 960);

  EXPECT_EQ(OnFreshTimestamp(kStart - 1, kStart + 960), 0);
  EXPECT_EQ(OnFreshTimestamp(kStart + 500, kStart + 960), 500);
  // Extrapolation with a large position and a long-stale timestamp.
  EXPECT_EQ(tracker_.OnTimestamp(kStart + 500, kNowUs - 5 * k48FramesUs, kNowUs,
                                 kStart + 960),
            240);
  EXPECT_EQ(
      tracker_.OnTimestamp(kStart, kNowUs - 100'000'000, kNowUs, kStart + 960),
      220);
  EXPECT_EQ(tracker_.frames_in_flight(), 0);
}

TEST_F(AaudioPresentedFrameTrackerTest, OtherSampleRate) {
  AaudioPresentedFrameTracker tracker(44'100);
  tracker.OnFramesCopied(0, 44'100);
  // 100 ms at 44.1 kHz.
  EXPECT_EQ(tracker.OnTimestamp(0, kNowUs - 100'000, kNowUs, 44'100), 4'410);
}

}  // namespace
}  // namespace starboard
