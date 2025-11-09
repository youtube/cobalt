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
#include <sstream>

#include "starboard/common/time.h"
#include "starboard/shared/starboard/player/job_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

using shared::starboard::player::JobThread;

constexpr int kMaxFrames = 2;

class DecoderStateTrackerTest : public ::testing::Test {
 protected:
  DecoderStateTrackerTest() : job_thread_("DecoderStateTrackerTest") {}

  ~DecoderStateTrackerTest() override {
    // Ensure decoder_state_tracker_ is destroyed on the job thread.
    job_thread_.ScheduleAndWait([this]() { decoder_state_tracker_.reset(); });
  }

  void CreateTracker(int max_frames,
                     DecoderStateTracker::StateChangedCB state_changed_cb) {
    job_thread_.ScheduleAndWait([=]() {
      decoder_state_tracker_ = std::make_unique<DecoderStateTracker>(
          max_frames, state_changed_cb, job_thread_.job_queue());
    });
  }

  JobThread job_thread_;
  std::unique_ptr<DecoderStateTracker> decoder_state_tracker_;
};

TEST_F(DecoderStateTrackerTest, InitialState) {
  CreateTracker(kMaxFrames, [] {});

  DecoderStateTracker::State status =
      decoder_state_tracker_->GetCurrentStateForTest();

  EXPECT_EQ(status.decoding_frames, 0);
  EXPECT_EQ(status.decoded_frames, 0);
}

TEST_F(DecoderStateTrackerTest, AddFrame) {
  CreateTracker(kMaxFrames, [] {});

  decoder_state_tracker_->AddFrame(0);
  DecoderStateTracker::State status =
      decoder_state_tracker_->GetCurrentStateForTest();

  EXPECT_EQ(status.decoding_frames, 1);
  EXPECT_EQ(status.decoded_frames, 0);
}

TEST_F(DecoderStateTrackerTest, AddFrameReturnsFalseWhenFull) {
  CreateTracker(kMaxFrames, [] {});
  for (int i = 0; i < kMaxFrames; ++i) {
    decoder_state_tracker_->AddFrame(i);
  }

  // Should still accept more because nothing is decoded yet.
  EXPECT_TRUE(decoder_state_tracker_->CanAcceptMore());
  decoder_state_tracker_->AddFrame(kMaxFrames);

  // Now decode one frame.
  decoder_state_tracker_->SetFrameDecoded(0);

  // Should be full now because decoded_frames > 0 and total_frames >=
  // kMaxFrames.
  EXPECT_FALSE(decoder_state_tracker_->CanAcceptMore());
}

TEST_F(DecoderStateTrackerTest, SetFrameDecoded) {
  CreateTracker(kMaxFrames, [] {});
  decoder_state_tracker_->AddFrame(0);

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

TEST_F(DecoderStateTrackerTest, OnFrameReleased) {
  CreateTracker(kMaxFrames, [] {});
  decoder_state_tracker_->AddFrame(0);
  decoder_state_tracker_->SetFrameDecoded(0);
  decoder_state_tracker_->AddFrame(1);
  decoder_state_tracker_->SetFrameDecoded(1);
  // Add one more to make it full (total 3 >= kMaxFrames 2, and decoded > 0)
  decoder_state_tracker_->AddFrame(2);

  ASSERT_FALSE(decoder_state_tracker_->CanAcceptMore());

  decoder_state_tracker_->OnFrameReleased(0, CurrentMonotonicTime() + 100'000);
  decoder_state_tracker_->OnFrameReleased(1, CurrentMonotonicTime() + 200'000);

  usleep(150'000);
  DecoderStateTracker::State status =
      decoder_state_tracker_->GetCurrentStateForTest();
  // Frame 0 released, Frame 1 still decoded, Frame 2 still decoding.
  EXPECT_EQ(status.decoded_frames, 1);
  EXPECT_EQ(status.decoding_frames, 1);
  // Total 2, max 2. Should be full?
  // IsFull_Locked: (decoding + decoded) >= max_frames_
  // 1 + 1 = 2 >= 2. Yes, it is full.
  EXPECT_FALSE(decoder_state_tracker_->CanAcceptMore());

  usleep(100'000);
  status = decoder_state_tracker_->GetCurrentStateForTest();
  // Frame 1 released. Frame 2 still decoding.
  EXPECT_EQ(status.decoded_frames, 0);
  EXPECT_EQ(status.decoding_frames, 1);
  // Total 1, max 2. Not full.
  EXPECT_TRUE(decoder_state_tracker_->CanAcceptMore());
}

TEST_F(DecoderStateTrackerTest, FrameReleasedCallback) {
  std::atomic_int counter{0};
  CreateTracker(kMaxFrames, [&counter]() { counter++; });

  decoder_state_tracker_->AddFrame(0);
  decoder_state_tracker_->AddFrame(1);
  decoder_state_tracker_->SetFrameDecoded(0);
  decoder_state_tracker_->SetFrameDecoded(1);
  // Total 2, max 2, decoded > 0 -> Full.

  decoder_state_tracker_->OnFrameReleased(0, CurrentMonotonicTime() + 100'000);
  decoder_state_tracker_->OnFrameReleased(1, CurrentMonotonicTime() + 200'000);

  usleep(50'000);  // at 50 msec
  ASSERT_EQ(counter.load(), 0);

  usleep(100'000);  // at 150 msec
  // Frame 0 released. Total 1. Was full (2), now not full (1). Callback should
  // fire.
  EXPECT_EQ(counter.load(), 1);

  usleep(100'000);  // at 250 msec
  // Frame 1 released. Total 0. Was not full (1), now not full (0). Callback
  // should NOT fire.
  EXPECT_EQ(counter.load(), 1);
}

TEST_F(DecoderStateTrackerTest, Reset) {
  CreateTracker(kMaxFrames, [] {});
  decoder_state_tracker_->AddFrame(0);
  decoder_state_tracker_->Reset();
  DecoderStateTracker::State status =
      decoder_state_tracker_->GetCurrentStateForTest();
  EXPECT_EQ(status.total_frames(), 0);
}

TEST_F(DecoderStateTrackerTest, KillSwitchDuplicateAddFrame) {
  std::atomic_int counter{0};
  CreateTracker(kMaxFrames, [&counter]() { counter++; });
  decoder_state_tracker_->AddFrame(0);
  decoder_state_tracker_->AddFrame(0);  // Should trigger kill switch

  EXPECT_TRUE(decoder_state_tracker_->CanAcceptMore());
  EXPECT_EQ(counter.load(), 1);  // Should have signaled
  EXPECT_EQ(decoder_state_tracker_->GetCurrentStateForTest().total_frames(), 0);
}

TEST_F(DecoderStateTrackerTest, UnknownSetFrameDecodedDoesNotEngageKillSwitch) {
  std::atomic_int counter{0};
  CreateTracker(kMaxFrames, [&counter]() { counter++; });
  decoder_state_tracker_->SetFrameDecoded(0);  // Should NOT trigger kill switch

  EXPECT_TRUE(decoder_state_tracker_->CanAcceptMore());
  EXPECT_EQ(counter.load(), 0);
  EXPECT_EQ(decoder_state_tracker_->GetCurrentStateForTest().total_frames(), 0);
}

TEST_F(DecoderStateTrackerTest, UnknownOnFrameReleasedDoesNotEngageKillSwitch) {
  std::atomic_int counter{0};
  CreateTracker(kMaxFrames, [&counter]() { counter++; });
  decoder_state_tracker_->OnFrameReleased(
      0, CurrentMonotonicTime());  // Should NOT trigger kill switch

  usleep(100'000);  // Wait for async task
  EXPECT_TRUE(decoder_state_tracker_->CanAcceptMore());
  EXPECT_EQ(counter.load(), 0);
  EXPECT_EQ(decoder_state_tracker_->GetCurrentStateForTest().total_frames(), 0);
}

TEST_F(DecoderStateTrackerTest, SetFrameDecodedIsCumulative) {
  CreateTracker(kMaxFrames, [] {});
  decoder_state_tracker_->AddFrame(0);
  decoder_state_tracker_->AddFrame(1);

  // Decoding frame 1 should also mark frame 0 as decoded.
  decoder_state_tracker_->SetFrameDecoded(1);
  DecoderStateTracker::State status =
      decoder_state_tracker_->GetCurrentStateForTest();

  EXPECT_EQ(status.decoding_frames, 0);
  EXPECT_EQ(status.decoded_frames, 2);
}

TEST_F(DecoderStateTrackerTest, OnFrameReleasedIsCumulative) {
  CreateTracker(kMaxFrames, [] {});
  decoder_state_tracker_->AddFrame(0);
  decoder_state_tracker_->AddFrame(1);
  decoder_state_tracker_->SetFrameDecoded(0);
  decoder_state_tracker_->SetFrameDecoded(1);

  // Releasing frame 1 should also release frame 0.
  decoder_state_tracker_->OnFrameReleased(1, CurrentMonotonicTime());

  usleep(100'000);  // Wait for async task
  DecoderStateTracker::State status =
      decoder_state_tracker_->GetCurrentStateForTest();
  EXPECT_EQ(status.total_frames(), 0);
}

TEST_F(DecoderStateTrackerTest, ResetReenablesAfterKillSwitch) {
  CreateTracker(kMaxFrames, [] {});
  decoder_state_tracker_->AddFrame(0);
  decoder_state_tracker_->AddFrame(0);  // Trigger kill switch
  EXPECT_EQ(decoder_state_tracker_->GetCurrentStateForTest().total_frames(), 0);

  decoder_state_tracker_->Reset();
  decoder_state_tracker_->AddFrame(0);
  EXPECT_EQ(decoder_state_tracker_->GetCurrentStateForTest().total_frames(), 1);
}

}  // namespace
}  // namespace starboard
