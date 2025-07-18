#include "starboard/shared/starboard/media/decoder_flow_control.h"

#include <sstream>

#include "starboard/common/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard::shared::starboard::media {
namespace {

constexpr int kMaxFrames = 2;

TEST(ThrottlingDecoderFlowControlTest, InitialState) {
  auto decoder_flow_control =
      DecoderFlowControl::CreateThrottling(kMaxFrames, 0, [] {});

  DecoderFlowControl::State status = decoder_flow_control->GetCurrentState();

  EXPECT_EQ(status.decoding_frames, 0);
  EXPECT_EQ(status.decoded_frames, 0);
  EXPECT_EQ(status.decoding_frames_high_water_mark, 0);
  EXPECT_EQ(status.decoded_frames_high_water_mark, 0);
  EXPECT_EQ(status.total_frames_high_water_mark, 0);
}

TEST(ThrottlingDecoderFlowControlTest, AddFrame) {
  auto decoder_flow_control =
      DecoderFlowControl::CreateThrottling(kMaxFrames, 0, [] {});

  ASSERT_TRUE(decoder_flow_control->AddFrame());
  DecoderFlowControl::State status = decoder_flow_control->GetCurrentState();

  EXPECT_EQ(status.decoding_frames, 1);
  EXPECT_EQ(status.decoded_frames, 0);
  EXPECT_EQ(status.decoding_frames_high_water_mark, 1);
  EXPECT_EQ(status.decoded_frames_high_water_mark, 0);
  EXPECT_EQ(status.total_frames_high_water_mark, 1);
}

TEST(ThrottlingDecoderFlowControlTest, AddFrameReturnsFalseWhenFull) {
  auto decoder_flow_control =
      DecoderFlowControl::CreateThrottling(kMaxFrames, 0, [] {});
  for (int i = 0; i < kMaxFrames; ++i) {
    ASSERT_TRUE(decoder_flow_control->AddFrame());
  }

  EXPECT_FALSE(decoder_flow_control->AddFrame());
}

TEST(ThrottlingDecoderFlowControlTest, SetFrameDecoded) {
  auto decoder_flow_control =
      DecoderFlowControl::CreateThrottling(kMaxFrames, 0, [] {});
  ASSERT_TRUE(decoder_flow_control->AddFrame());

  ASSERT_TRUE(decoder_flow_control->SetFrameDecoded());
  DecoderFlowControl::State status = decoder_flow_control->GetCurrentState();

  EXPECT_EQ(status.decoding_frames, 0);
  EXPECT_EQ(status.decoded_frames, 1);
  EXPECT_EQ(status.decoding_frames_high_water_mark, 1);
  EXPECT_EQ(status.decoded_frames_high_water_mark, 1);
  EXPECT_EQ(status.total_frames_high_water_mark, 1);
}

TEST(ThrottlingDecoderFlowControlTest, SetFrameDecodedReturnsFalseWhenEmpty) {
  auto decoder_flow_control =
      DecoderFlowControl::CreateThrottling(kMaxFrames, 0, [] {});

  EXPECT_FALSE(decoder_flow_control->SetFrameDecoded());
}

TEST(ThrottlingDecoderFlowControlTest, HighWaterMark) {
  auto decoder_flow_control =
      DecoderFlowControl::CreateThrottling(kMaxFrames, 0, [] {});
  ASSERT_TRUE(decoder_flow_control->AddFrame());
  ASSERT_TRUE(decoder_flow_control->AddFrame());
  ASSERT_TRUE(decoder_flow_control->SetFrameDecoded());
  ASSERT_TRUE(decoder_flow_control->SetFrameDecoded());
  ASSERT_TRUE(decoder_flow_control->ReleaseFrameAt(CurrentMonotonicTime()));

  usleep(10'000);
  DecoderFlowControl::State status = decoder_flow_control->GetCurrentState();

  EXPECT_EQ(status.decoding_frames, 0);
  EXPECT_EQ(status.decoded_frames, 1);
  EXPECT_EQ(status.decoding_frames_high_water_mark, 2);
  EXPECT_EQ(status.decoded_frames_high_water_mark, 2);
  EXPECT_EQ(status.total_frames_high_water_mark, 2);
}

TEST(ThrottlingDecoderFlowControlTest, StreamInsertionOperator) {
  DecoderFlowControl::State status;
  status.decoding_frames = 1;
  status.decoded_frames = 2;
  status.decoding_frames_high_water_mark = 3;
  status.decoded_frames_high_water_mark = 4;
  status.total_frames_high_water_mark = 5;
  status.min_decoding_time_us = 10'000;
  status.max_decoding_time_us = 20'000;
  status.avg_decoding_time_us = 15'000;

  std::stringstream ss;
  ss << status;

  EXPECT_EQ(ss.str(),
            "{decoding: 1, decoded: 2, decoding (hw): 3, decoded (hw): 4, "
            "total (hw): 5, min decoding(msec): 10, max decoding(msec): 20, "
            "avg decoding(msec): 15}");
}

TEST(ThrottlingDecoderFlowControlTest, ReleaseFrameAt) {
  auto decoder_flow_control =
      DecoderFlowControl::CreateThrottling(kMaxFrames, 0, [] {});
  ASSERT_TRUE(decoder_flow_control->AddFrame());
  ASSERT_TRUE(decoder_flow_control->SetFrameDecoded());
  ASSERT_TRUE(decoder_flow_control->AddFrame());
  ASSERT_TRUE(decoder_flow_control->SetFrameDecoded());

  ASSERT_TRUE(decoder_flow_control->IsFull());

  decoder_flow_control->ReleaseFrameAt(CurrentMonotonicTime() + 100'000);
  decoder_flow_control->ReleaseFrameAt(CurrentMonotonicTime() + 200'000);

  usleep(150'000);
  DecoderFlowControl::State status = decoder_flow_control->GetCurrentState();
  EXPECT_EQ(status.decoded_frames, 1);
  EXPECT_FALSE(decoder_flow_control->IsFull());

  usleep(100'000);
  status = decoder_flow_control->GetCurrentState();
  EXPECT_EQ(status.decoded_frames, 0);
  EXPECT_FALSE(decoder_flow_control->IsFull());
}

TEST(ThrottlingDecoderFlowControlTest, LongIntervalNotBlockDestruction) {
  auto decoder_flow_control =
      DecoderFlowControl::CreateThrottling(kMaxFrames, 1'000'000'000, [] {});
}

TEST(ThrottlingDecoderFlowControlTest, DecodingTimeStats) {
  auto decoder_flow_control =
      DecoderFlowControl::CreateThrottling(kMaxFrames, 0, [] {});

  ASSERT_TRUE(decoder_flow_control->AddFrame());
  usleep(10'000);
  ASSERT_TRUE(decoder_flow_control->SetFrameDecoded());

  ASSERT_TRUE(decoder_flow_control->AddFrame());
  usleep(20'000);
  ASSERT_TRUE(decoder_flow_control->SetFrameDecoded());

  DecoderFlowControl::State status = decoder_flow_control->GetCurrentState();
  EXPECT_GE(status.min_decoding_time_us, 10'000);
  EXPECT_LE(status.min_decoding_time_us, 15'000);
  EXPECT_GE(status.max_decoding_time_us, 20'000);
  EXPECT_LE(status.max_decoding_time_us, 25'000);
  EXPECT_GE(status.avg_decoding_time_us, 15'000);
  EXPECT_LE(status.avg_decoding_time_us, 20'000);
}

TEST(ThrottlingDecoderFlowControlTest, FrameReleasedCallback) {
  int counter = 0;
  auto decoder_flow_control = DecoderFlowControl::CreateThrottling(
      kMaxFrames, 0, [&counter]() { counter++; });

  ASSERT_TRUE(decoder_flow_control->AddFrame());
  ASSERT_TRUE(decoder_flow_control->AddFrame());
  ASSERT_TRUE(decoder_flow_control->SetFrameDecoded());
  ASSERT_TRUE(decoder_flow_control->SetFrameDecoded());
  decoder_flow_control->ReleaseFrameAt(CurrentMonotonicTime() + 100'000);
  decoder_flow_control->ReleaseFrameAt(CurrentMonotonicTime() + 200'000);

  usleep(50'000);  // at 50 msec
  ASSERT_EQ(counter, 0);

  usleep(100'000);  // at 150 msec
  EXPECT_EQ(counter, 1);

  usleep(100'000);  // at 250 msec
  EXPECT_EQ(counter, 2);
}

}  // namespace
}  // namespace starboard::shared::starboard::media
