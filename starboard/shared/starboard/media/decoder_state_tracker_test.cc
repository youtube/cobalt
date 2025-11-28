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

#include <atomic>
#include <optional>
#include <sstream>

#include "starboard/common/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

constexpr int kMaxFrames = 2;

class DecoderStateTrackerTest : public ::testing::Test {
 protected:
  void CreateTracker(int max_frames,
                     DecoderStateTracker::StateChangedCB state_changed_cb) {
    decoder_state_tracker_ = std::make_unique<DecoderStateTracker>(
        max_frames, std::move(state_changed_cb),
        /*log_interval_us=*/std::nullopt);
  }

  std::unique_ptr<DecoderStateTracker> decoder_state_tracker_;
};

TEST_F(DecoderStateTrackerTest, InitialState) {
  CreateTracker(kMaxFrames, [] {});

  DecoderStateTracker::State status =
      decoder_state_tracker_->GetCurrentStateForTest();

  EXPECT_EQ(status.decoding_frames, 0);
  EXPECT_EQ(status.decoded_frames, 0);
}

TEST_F(DecoderStateTrackerTest, SetFrameAdded) {
  CreateTracker(kMaxFrames, [] {});

  decoder_state_tracker_->SetFrameAdded(0);
  DecoderStateTracker::State status =
      decoder_state_tracker_->GetCurrentStateForTest();

  EXPECT_EQ(status.decoding_frames, 1);
  EXPECT_EQ(status.decoded_frames, 0);
}

TEST_F(DecoderStateTrackerTest, SetFrameAddedReturnsFalseWhenFull) {
  CreateTracker(kMaxFrames, [] {});
  for (int i = 0; i < kMaxFrames; ++i) {
    decoder_state_tracker_->SetFrameAdded(i);
  }

  EXPECT_TRUE(decoder_state_tracker_->CanAcceptMore());
  decoder_state_tracker_->SetFrameAdded(kMaxFrames);

  decoder_state_tracker_->SetFrameDecoded(0);

  EXPECT_FALSE(decoder_state_tracker_->CanAcceptMore());
}

TEST_F(DecoderStateTrackerTest, SetFrameDecoded) {
  CreateTracker(kMaxFrames, [] {});
  decoder_state_tracker_->SetFrameAdded(0);

  decoder_state_tracker_->SetFrameDecoded(0);
  DecoderStateTracker::State status =
      decoder_state_tracker_->GetCurrentStateForTest();

  EXPECT_EQ(status.decoding_frames, 0);
  EXPECT_EQ(status.decoded_frames, 1);
}

TEST_F(DecoderStateTrackerTest, StreamInsertionOperator) {
  DecoderStateTracker::State status;
  status.decoding_frames = 1;
  status.decoded_frames = 2;

  std::stringstream ss;
  ss << status;

  EXPECT_EQ(ss.str(), "{decoding: 1, decoded: 2, total: 3}");
}

TEST_F(DecoderStateTrackerTest, SetFrameReleasedAt) {
  CreateTracker(kMaxFrames, [] {});
  decoder_state_tracker_->SetFrameAdded(0);
  decoder_state_tracker_->SetFrameDecoded(0);
  decoder_state_tracker_->SetFrameAdded(1);
  decoder_state_tracker_->SetFrameDecoded(1);
  decoder_state_tracker_->SetFrameAdded(2);

  ASSERT_FALSE(decoder_state_tracker_->CanAcceptMore());

  decoder_state_tracker_->SetFrameReleasedAt(0,
                                             CurrentMonotonicTime() + 100'000);
  decoder_state_tracker_->SetFrameReleasedAt(1,
                                             CurrentMonotonicTime() + 200'000);

  usleep(150'000);
  DecoderStateTracker::State status =
      decoder_state_tracker_->GetCurrentStateForTest();
  EXPECT_EQ(status.decoded_frames, 1);
  EXPECT_EQ(status.decoding_frames, 1);
  EXPECT_TRUE(decoder_state_tracker_->CanAcceptMore());

  usleep(100'000);
  status = decoder_state_tracker_->GetCurrentStateForTest();
  EXPECT_EQ(status.decoded_frames, 0);
  EXPECT_EQ(status.decoding_frames, 1);
  EXPECT_TRUE(decoder_state_tracker_->CanAcceptMore());
}

TEST_F(DecoderStateTrackerTest, FrameReleasedCallback) {
  std::atomic_int counter{0};
  CreateTracker(kMaxFrames, [&counter]() { counter++; });

  decoder_state_tracker_->SetFrameAdded(0);
  decoder_state_tracker_->SetFrameAdded(1);
  decoder_state_tracker_->SetFrameDecoded(0);
  decoder_state_tracker_->SetFrameDecoded(1);

  decoder_state_tracker_->SetFrameReleasedAt(0,
                                             CurrentMonotonicTime() + 100'000);
  decoder_state_tracker_->SetFrameReleasedAt(1,
                                             CurrentMonotonicTime() + 200'000);

  usleep(50'000);  // at 50 msec
  ASSERT_EQ(counter.load(), 0);

  usleep(100'000);  // at 150 msec
  EXPECT_GE(counter.load(), 1);

  usleep(100'000);  // at 250 msec
  EXPECT_GE(counter.load(), 2);
}

TEST_F(DecoderStateTrackerTest, Reset) {
  CreateTracker(kMaxFrames, [] {});
  decoder_state_tracker_->SetFrameAdded(0);
  decoder_state_tracker_->Reset();
  DecoderStateTracker::State status =
      decoder_state_tracker_->GetCurrentStateForTest();
  EXPECT_EQ(status.total_frames(), 0);
}

TEST_F(DecoderStateTrackerTest, KillSwitchDuplicateSetFrameAdded) {
  std::atomic_int counter{0};
  CreateTracker(kMaxFrames, [&counter]() { counter++; });
  decoder_state_tracker_->SetFrameAdded(0);
  decoder_state_tracker_->SetFrameAdded(0);

  EXPECT_TRUE(decoder_state_tracker_->CanAcceptMore());
  EXPECT_EQ(counter.load(), 1);
  EXPECT_EQ(decoder_state_tracker_->GetCurrentStateForTest().total_frames(), 0);
}

TEST_F(DecoderStateTrackerTest, UnknownSetFrameDecodedDoesNotEngageKillSwitch) {
  std::atomic_int counter{0};
  CreateTracker(kMaxFrames, [&counter]() { counter++; });
  decoder_state_tracker_->SetFrameDecoded(0);

  EXPECT_TRUE(decoder_state_tracker_->CanAcceptMore());
  EXPECT_EQ(counter.load(), 0);
  EXPECT_EQ(decoder_state_tracker_->GetCurrentStateForTest().total_frames(), 0);
}

TEST_F(DecoderStateTrackerTest,
       UnknownSetFrameReleasedAtDoesNotEngageKillSwitch) {
  std::atomic_int counter{0};
  CreateTracker(kMaxFrames, [&counter]() { counter++; });
  decoder_state_tracker_->SetFrameReleasedAt(0, CurrentMonotonicTime());

  usleep(100'000);
  EXPECT_TRUE(decoder_state_tracker_->CanAcceptMore());
  EXPECT_EQ(counter.load(), 1);
  EXPECT_EQ(decoder_state_tracker_->GetCurrentStateForTest().total_frames(), 0);
}

TEST_F(DecoderStateTrackerTest, SetFrameDecodedIsCumulative) {
  CreateTracker(kMaxFrames, [] {});
  decoder_state_tracker_->SetFrameAdded(0);
  decoder_state_tracker_->SetFrameAdded(1);

  decoder_state_tracker_->SetFrameDecoded(1);
  DecoderStateTracker::State status =
      decoder_state_tracker_->GetCurrentStateForTest();

  EXPECT_EQ(status.decoding_frames, 0);
  EXPECT_EQ(status.decoded_frames, 2);
}

TEST_F(DecoderStateTrackerTest, SetFrameReleasedAtIsCumulative) {
  CreateTracker(kMaxFrames, [] {});
  decoder_state_tracker_->SetFrameAdded(0);
  decoder_state_tracker_->SetFrameAdded(1);
  decoder_state_tracker_->SetFrameDecoded(0);
  decoder_state_tracker_->SetFrameDecoded(1);

  decoder_state_tracker_->SetFrameReleasedAt(1, CurrentMonotonicTime());

  usleep(100'000);
  DecoderStateTracker::State status =
      decoder_state_tracker_->GetCurrentStateForTest();
  EXPECT_EQ(status.total_frames(), 0);
}

TEST_F(DecoderStateTrackerTest, ResetReenablesAfterKillSwitch) {
  CreateTracker(kMaxFrames, [] {});
  decoder_state_tracker_->SetFrameAdded(0);
  decoder_state_tracker_->SetFrameAdded(0);
  EXPECT_EQ(decoder_state_tracker_->GetCurrentStateForTest().total_frames(), 0);

  decoder_state_tracker_->Reset();
  decoder_state_tracker_->SetFrameAdded(0);
  EXPECT_EQ(decoder_state_tracker_->GetCurrentStateForTest().total_frames(), 1);
}

TEST_F(DecoderStateTrackerTest, DynamicAdaptation) {
  CreateTracker(2, [] {});

  decoder_state_tracker_->SetFrameAdded(0);
  decoder_state_tracker_->SetFrameDecoded(0);
  decoder_state_tracker_->SetFrameAdded(1);
  EXPECT_FALSE(decoder_state_tracker_->CanAcceptMore());

  decoder_state_tracker_->SetFrameReleasedAt(0, CurrentMonotonicTime());
  usleep(100'000);
  EXPECT_TRUE(decoder_state_tracker_->CanAcceptMore());

  decoder_state_tracker_->SetFrameAdded(2);
  decoder_state_tracker_->SetFrameDecoded(2);
  decoder_state_tracker_->SetFrameAdded(3);
  decoder_state_tracker_->SetFrameAdded(4);
  EXPECT_FALSE(decoder_state_tracker_->CanAcceptMore());

  decoder_state_tracker_->SetFrameDecoded(1);
  decoder_state_tracker_->SetFrameDecoded(3);
  decoder_state_tracker_->SetFrameReleasedAt(1, CurrentMonotonicTime());
  decoder_state_tracker_->SetFrameReleasedAt(2, CurrentMonotonicTime());
  decoder_state_tracker_->SetFrameReleasedAt(3, CurrentMonotonicTime());
  usleep(100'000);
  EXPECT_TRUE(decoder_state_tracker_->CanAcceptMore());
}

}  // namespace
}  // namespace starboard
