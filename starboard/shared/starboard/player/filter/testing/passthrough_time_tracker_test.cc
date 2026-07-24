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

#include "starboard/shared/starboard/player/filter/passthrough_time_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace {

constexpr int kSampleRate = 48'000;
// 1536 samples per AC-3 / E-AC-3 syncframe at 48kHz = exactly 32,000
// microseconds.
constexpr int kSyncframeSamples = 1536;
constexpr int64_t kSyncframeDurationUsec = 32'000;

class PassthroughTimeTrackerTest : public ::testing::Test {
 protected:
  PassthroughTimeTracker tracker_;
};

TEST_F(PassthroughTimeTrackerTest, InitialStateAndSeekReset) {
  // Before any buffers are written, returns the fallback seek time.
  tracker_.Reset(/*seek_to_time=*/10'000'000);
  EXPECT_EQ(tracker_.GetCurrentMediaTime(0, kSampleRate, 10'000'000),
            10'000'000);
}

TEST_F(PassthroughTimeTrackerTest, NormalLinearPlaybackWithoutDiscards) {
  // Write 3 consecutive syncframes without any discards.
  // Buffer 0: [0 .. 1536] samples -> PTS 0
  tracker_.OnBufferWritten(0, 0, kSyncframeDurationUsec, 0, 0);
  // Buffer 1: [1536 .. 3072] samples -> PTS 32,000 us
  tracker_.OnBufferWritten(kSyncframeSamples, kSyncframeDurationUsec,
                           kSyncframeDurationUsec, 0, 0);
  // Buffer 2: [3072 .. 4608] samples -> PTS 64,000 us
  tracker_.OnBufferWritten(kSyncframeSamples * 2, kSyncframeDurationUsec * 2,
                           kSyncframeDurationUsec, 0, 0);

  // Playhead at 0 samples (start of Buffer 0) -> media time 0.
  EXPECT_EQ(tracker_.GetCurrentMediaTime(0, kSampleRate, 0), 0);

  // Playhead halfway through Buffer 0 (768 samples = 16ms) -> media time 16,000
  // us.
  EXPECT_EQ(tracker_.GetCurrentMediaTime(kSyncframeSamples / 2, kSampleRate, 0),
            16'000);

  // Playhead at start of Buffer 1 (1536 samples) -> media time 32,000 us.
  EXPECT_EQ(tracker_.GetCurrentMediaTime(kSyncframeSamples, kSampleRate, 0),
            32'000);

  // Playhead halfway through Buffer 1 (2304 samples) -> media time 48,000 us.
  EXPECT_EQ(tracker_.GetCurrentMediaTime(
                kSyncframeSamples + kSyncframeSamples / 2, kSampleRate, 0),
            48'000);
}

TEST_F(PassthroughTimeTrackerTest,
       FrontDiscardClampsMediaTimeUntilPreamblePlays) {
  // Write a single syncframe whose first 10,000 us (480 samples) are trimmed
  // preamble. The buffer's PTS on the media timeline is 100,000 us.
  constexpr int64_t kDiscardFrontUsec = 10'000;
  constexpr int kDiscardFrontSamples = 480;  // 10ms at 48kHz

  tracker_.OnBufferWritten(0, 100'000, kSyncframeDurationUsec,
                           kDiscardFrontUsec, 0);

  // 1. Playhead at 0 samples: front discard padding begins playing out of
  // speaker. Media time must stay clamped at the buffer's PTS (100,000 us)
  // rather than running ahead.
  EXPECT_EQ(tracker_.GetCurrentMediaTime(0, kSampleRate, 0), 100'000);

  // 2. Playhead at 240 samples (5ms into front padding): still in trimmed
  // region.
  EXPECT_EQ(tracker_.GetCurrentMediaTime(240, kSampleRate, 0), 100'000);

  // 3. Playhead at exactly 480 samples (10ms): front discard region finished!
  // Valid audio starts playing out of speaker -> media time is exactly 100,000
  // us.
  EXPECT_EQ(tracker_.GetCurrentMediaTime(kDiscardFrontSamples, kSampleRate, 0),
            100'000);

  // 4. Playhead at 960 samples (20ms total played -> 10ms into valid audio).
  // Media time = PTS (100,000) + (20,000 played - 10,000 discarded) = 110,000
  // us.
  EXPECT_EQ(tracker_.GetCurrentMediaTime(960, kSampleRate, 0), 110'000);
}

TEST_F(PassthroughTimeTrackerTest, BackDiscardClampsMediaTimeAtValidBufferEnd) {
  // Write a syncframe whose last 8,000 us (384 samples) are discarded trailing
  // padding.
  constexpr int64_t kDiscardBackUsec = 8'000;

  tracker_.OnBufferWritten(0, 200'000, kSyncframeDurationUsec, 0,
                           kDiscardBackUsec);

  // 1. Playhead 12ms into buffer: inside valid region -> 212,000 us.
  EXPECT_EQ(tracker_.GetCurrentMediaTime(12 * 48, kSampleRate, 0), 212'000);

  // 2. Playhead at 24ms (exactly at valid end): media time reaches max valid
  // PTS -> 224,000 us.
  EXPECT_EQ(tracker_.GetCurrentMediaTime(24 * 48, kSampleRate, 0), 224'000);

  // 3. Playhead at 28ms (4ms into discarded back padding playing from speaker):
  // Media time must NOT overshoot into unwanted timeline; stays clamped at
  // 224,000 us.
  EXPECT_EQ(tracker_.GetCurrentMediaTime(28 * 48, kSampleRate, 0), 224'000);
}

TEST_F(PassthroughTimeTrackerTest, CombinedFrontAndBackDiscard) {
  // A single syncframe where both front (6ms) and back (6ms) are discarded out
  // of 32ms. Valid interval is from 6ms to 26ms (total 20ms valid duration).
  constexpr int64_t kDiscardFrontUsec = 6'000;
  constexpr int64_t kDiscardBackUsec = 6'000;

  tracker_.OnBufferWritten(0, 300'000, kSyncframeDurationUsec,
                           kDiscardFrontUsec, kDiscardBackUsec);

  // During front discard (3ms in): clamped to PTS.
  EXPECT_EQ(tracker_.GetCurrentMediaTime(3 * 48, kSampleRate, 0), 300'000);

  // During valid middle (16ms total in -> 10ms valid audio played): PTS + 10ms.
  EXPECT_EQ(tracker_.GetCurrentMediaTime(16 * 48, kSampleRate, 0), 310'000);

  // During back discard (30ms total in -> past valid 20ms duration): clamped to
  // PTS + 20ms.
  EXPECT_EQ(tracker_.GetCurrentMediaTime(30 * 48, kSampleRate, 0), 320'000);
}

TEST_F(PassthroughTimeTrackerTest,
       QueueEvictionEnsuresNoDriftAcrossMultipleBuffers) {
  // Buffer 0 has a 10ms front discard.
  tracker_.OnBufferWritten(0, 0, kSyncframeDurationUsec, 10'000, 0);
  // Buffer 1 is a normal buffer following immediately on hardware sample 1536.
  // Its PTS on the media timeline is 22,000 us (32ms minus the 10ms discarded
  // in Buffer 0).
  tracker_.OnBufferWritten(kSyncframeSamples, 22'000, kSyncframeDurationUsec, 0,
                           0);

  // When playhead advances to sample 2304 (halfway through Buffer 1):
  // Buffer 0 should be evicted, and Buffer 1 calculates cleanly from its own
  // PTS: 22,000 us + 16,000 us = 38,000 us.
  EXPECT_EQ(tracker_.GetCurrentMediaTime(
                kSyncframeSamples + kSyncframeSamples / 2, kSampleRate, 0),
            38'000);
}

}  // namespace
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
