#include "starboard/shared/starboard/media/decoder_state_tracker.h"

#include <sstream>

#include "starboard/common/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

constexpr int kMaxFrames = 2;

TEST(DecoderStateTrackerTest, InitialState) {
  auto decoder_state_tracker =
      DecoderStateTracker::CreateThrottling(kMaxFrames, 0, [] {});

  DecoderStateTracker::State status = decoder_state_tracker->GetCurrentState();

  EXPECT_EQ(status.decoding_frames, 0);
  EXPECT_EQ(status.decoded_frames, 0);
}

TEST(DecoderStateTrackerTest, AddFrame) {
  auto decoder_state_tracker =
      DecoderStateTracker::CreateThrottling(kMaxFrames, 0, [] {});

  ASSERT_TRUE(decoder_state_tracker->AddFrame(0));
  DecoderStateTracker::State status = decoder_state_tracker->GetCurrentState();

  EXPECT_EQ(status.decoding_frames, 1);
  EXPECT_EQ(status.decoded_frames, 0);
}

TEST(DecoderStateTrackerTest, AddFrameReturnsFalseWhenFull) {
  auto decoder_state_tracker =
      DecoderStateTracker::CreateThrottling(kMaxFrames, 0, [] {});
  for (int i = 0; i < kMaxFrames; ++i) {
    ASSERT_TRUE(decoder_state_tracker->AddFrame(0));
  }

  EXPECT_FALSE(decoder_state_tracker->CanAcceptMore());
}

TEST(DecoderStateTrackerTest, SetFrameDecoded) {
  auto decoder_state_tracker =
      DecoderStateTracker::CreateThrottling(kMaxFrames, 0, [] {});
  ASSERT_TRUE(decoder_state_tracker->AddFrame(0));

  ASSERT_TRUE(decoder_state_tracker->SetFrameDecoded(0));
  DecoderStateTracker::State status = decoder_state_tracker->GetCurrentState();

  EXPECT_EQ(status.decoding_frames, 0);
  EXPECT_EQ(status.decoded_frames, 1);
}

TEST(DecoderStateTrackerTest, SetFrameDecodedReturnsFalseWhenEmpty) {
  auto decoder_state_tracker =
      DecoderStateTracker::CreateThrottling(kMaxFrames, 0, [] {});

  EXPECT_FALSE(decoder_state_tracker->SetFrameDecoded(0));
}

TEST(DecoderStateTrackerTest, StreamInsertionOperator) {
  DecoderStateTracker::State status;
  status.decoding_frames = 1;
  status.decoded_frames = 2;

  std::stringstream ss;
  ss << status;

  EXPECT_EQ(ss.str(), "{decoding: 1, decoded: 2}");
}

TEST(DecoderStateTrackerTest, ReleaseFrameAt) {
  auto decoder_state_tracker =
      DecoderStateTracker::CreateThrottling(kMaxFrames, 0, [] {});
  ASSERT_TRUE(decoder_state_tracker->AddFrame(0));
  ASSERT_TRUE(decoder_state_tracker->SetFrameDecoded(0));
  ASSERT_TRUE(decoder_state_tracker->AddFrame(0));
  ASSERT_TRUE(decoder_state_tracker->SetFrameDecoded(0));

  ASSERT_FALSE(decoder_state_tracker->CanAcceptMore());

  decoder_state_tracker->ReleaseFrameAt(CurrentMonotonicTime() + 100'000);
  decoder_state_tracker->ReleaseFrameAt(CurrentMonotonicTime() + 200'000);

  usleep(150'000);
  DecoderStateTracker::State status = decoder_state_tracker->GetCurrentState();
  EXPECT_EQ(status.decoded_frames, 1);
  EXPECT_TRUE(decoder_state_tracker->CanAcceptMore());

  usleep(100'000);
  status = decoder_state_tracker->GetCurrentState();
  EXPECT_EQ(status.decoded_frames, 0);
  EXPECT_TRUE(decoder_state_tracker->CanAcceptMore());
}

TEST(DecoderStateTrackerTest, LongIntervalNotBlockDestruction) {
  auto decoder_state_tracker =
      DecoderStateTracker::CreateThrottling(kMaxFrames, 1'000'000'000, [] {});
}

TEST(DecoderStateTrackerTest, FrameReleasedCallback) {
  int counter = 0;
  auto decoder_state_tracker = DecoderStateTracker::CreateThrottling(
      kMaxFrames, 0, [&counter]() { counter++; });

  ASSERT_TRUE(decoder_state_tracker->AddFrame(0));
  ASSERT_TRUE(decoder_state_tracker->AddFrame(0));
  ASSERT_TRUE(decoder_state_tracker->SetFrameDecoded(0));
  ASSERT_TRUE(decoder_state_tracker->SetFrameDecoded(0));
  decoder_state_tracker->ReleaseFrameAt(CurrentMonotonicTime() + 100'000);
  decoder_state_tracker->ReleaseFrameAt(CurrentMonotonicTime() + 200'000);

  usleep(50'000);  // at 50 msec
  ASSERT_EQ(counter, 0);

  usleep(100'000);  // at 150 msec
  EXPECT_EQ(counter, 1);

  usleep(100'000);  // at 250 msec
  EXPECT_EQ(counter, 1);
}

TEST(DecoderStateTrackerTest, AllMethodsNoOp) {
  auto decoder_state_tracker = DecoderStateTracker::CreateNoOp();
  EXPECT_TRUE(decoder_state_tracker->AddFrame(0));
  EXPECT_TRUE(decoder_state_tracker->SetFrameDecoded(0));
  EXPECT_TRUE(decoder_state_tracker->ReleaseFrameAt(0));
  EXPECT_TRUE(decoder_state_tracker->CanAcceptMore());
  auto status = decoder_state_tracker->GetCurrentState();
  EXPECT_EQ(status.decoding_frames, 0);
  EXPECT_EQ(status.decoded_frames, 0);
}

}  // namespace
}  // namespace starboard
