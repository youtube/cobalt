// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/decoder_state_tracker.h"

#include <sstream>

#include "starboard/common/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

constexpr int kMaxFrames = 2;

TEST(DecoderStateTrackerTest, InitialState) {
  auto decoder_state_tracker =
      std::make_unique<DecoderStateTracker>(kMaxFrames, [] {});

  DecoderStateTracker::State status = decoder_state_tracker->GetCurrentState();

  EXPECT_EQ(status.decoding_frames, 0);
  EXPECT_EQ(status.decoded_frames, 0);
}

TEST(DecoderStateTrackerTest, AddFrame) {
  auto decoder_state_tracker =
      std::make_unique<DecoderStateTracker>(kMaxFrames, [] {});

  decoder_state_tracker->AddFrame(0);
  DecoderStateTracker::State status = decoder_state_tracker->GetCurrentState();

  EXPECT_EQ(status.decoding_frames, 1);
  EXPECT_EQ(status.decoded_frames, 0);
}

TEST(DecoderStateTrackerTest, AddFrameReturnsFalseWhenFull) {
  auto decoder_state_tracker =
      std::make_unique<DecoderStateTracker>(kMaxFrames, [] {});
  for (int i = 0; i < kMaxFrames; ++i) {
    decoder_state_tracker->AddFrame(i);
  }

  // Should still accept more because nothing is decoded yet.
  EXPECT_TRUE(decoder_state_tracker->CanAcceptMore());
  decoder_state_tracker->AddFrame(kMaxFrames);

  // Now decode one frame.
  decoder_state_tracker->SetFrameDecoded(0);

  // Should be full now because decoded_frames > 0 and total_frames >=
  // kMaxFrames.
  EXPECT_FALSE(decoder_state_tracker->CanAcceptMore());
}

TEST(DecoderStateTrackerTest, SetFrameDecoded) {
  auto decoder_state_tracker =
      std::make_unique<DecoderStateTracker>(kMaxFrames, [] {});
  decoder_state_tracker->AddFrame(0);

  decoder_state_tracker->SetFrameDecoded(0);
  DecoderStateTracker::State status = decoder_state_tracker->GetCurrentState();

  EXPECT_EQ(status.decoding_frames, 0);
  EXPECT_EQ(status.decoded_frames, 1);
}

TEST(DecoderStateTrackerTest, StreamInsertionOperator) {
  DecoderStateTracker::State status;
  status.decoding_frames = 1;
  status.decoded_frames = 2;

  std::stringstream ss;
  ss << status;

  EXPECT_EQ(ss.str(), "{decoding: 1, decoded: 2, total: 3}");
}

TEST(DecoderStateTrackerTest, OnFrameReleased) {
  auto decoder_state_tracker =
      std::make_unique<DecoderStateTracker>(kMaxFrames, [] {});
  decoder_state_tracker->AddFrame(0);
  decoder_state_tracker->SetFrameDecoded(0);
  decoder_state_tracker->AddFrame(1);
  decoder_state_tracker->SetFrameDecoded(1);
  // Add one more to make it full (total 3 >= kMaxFrames 2, and decoded > 0)
  decoder_state_tracker->AddFrame(2);

  ASSERT_FALSE(decoder_state_tracker->CanAcceptMore());

  decoder_state_tracker->OnFrameReleased(0, CurrentMonotonicTime() + 100'000);
  decoder_state_tracker->OnFrameReleased(1, CurrentMonotonicTime() + 200'000);

  usleep(150'000);
  DecoderStateTracker::State status = decoder_state_tracker->GetCurrentState();
  // Frame 0 released, Frame 1 still decoded, Frame 2 still decoding.
  EXPECT_EQ(status.decoded_frames, 1);
  EXPECT_EQ(status.decoding_frames, 1);
  // Total 2, max 2. Should be full?
  // IsFull_Locked: (decoding + decoded) >= max_frames_
  // 1 + 1 = 2 >= 2. Yes, it is full.
  EXPECT_FALSE(decoder_state_tracker->CanAcceptMore());

  usleep(100'000);
  status = decoder_state_tracker->GetCurrentState();
  // Frame 1 released. Frame 2 still decoding.
  EXPECT_EQ(status.decoded_frames, 0);
  EXPECT_EQ(status.decoding_frames, 1);
  // Total 1, max 2. Not full.
  EXPECT_TRUE(decoder_state_tracker->CanAcceptMore());
}

TEST(DecoderStateTrackerTest, FrameReleasedCallback) {
  int counter = 0;
  auto decoder_state_tracker = std::make_unique<DecoderStateTracker>(
      kMaxFrames, [&counter]() { counter++; });

  decoder_state_tracker->AddFrame(0);
  decoder_state_tracker->AddFrame(1);
  decoder_state_tracker->SetFrameDecoded(0);
  decoder_state_tracker->SetFrameDecoded(1);
  // Total 2, max 2, decoded > 0 -> Full.

  decoder_state_tracker->OnFrameReleased(0, CurrentMonotonicTime() + 100'000);
  decoder_state_tracker->OnFrameReleased(1, CurrentMonotonicTime() + 200'000);

  usleep(50'000);  // at 50 msec
  ASSERT_EQ(counter, 0);

  usleep(100'000);  // at 150 msec
  // Frame 0 released. Total 1. Was full (2), now not full (1). Callback should
  // fire.
  EXPECT_EQ(counter, 1);

  usleep(100'000);  // at 250 msec
  // Frame 1 released. Total 0. Was not full (1), now not full (0). Callback
  // should NOT fire.
  EXPECT_EQ(counter, 1);
}

}  // namespace
}  // namespace starboard
