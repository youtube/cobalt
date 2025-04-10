/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sender_video_frame_transformer_delegate.h"

#include <utility>

#include "rtc_base/event.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_frame_transformer.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::SaveArg;
using ::testing::WithoutArgs;

class MockRTPVideoFrameSenderInterface : public RTPVideoFrameSenderInterface {
 public:
  MOCK_METHOD(bool,
              SendVideo,
              (int payload_type,
               absl::optional<VideoCodecType> codec_type,
               uint32_t rtp_timestamp,
               int64_t capture_time_ms,
               rtc::ArrayView<const uint8_t> payload,
               RTPVideoHeader video_header,
               absl::optional<int64_t> expected_retransmission_time_ms,
               std::vector<uint32_t> csrcs),
              (override));

  MOCK_METHOD(void,
              SetVideoStructureAfterTransformation,
              (const FrameDependencyStructure* video_structure),
              (override));
  MOCK_METHOD(void,
              SetVideoLayersAllocationAfterTransformation,
              (VideoLayersAllocation allocation),
              (override));
};

class RtpSenderVideoFrameTransformerDelegateTest : public ::testing::Test {
 protected:
  RtpSenderVideoFrameTransformerDelegateTest()
      : frame_transformer_(rtc::make_ref_counted<MockFrameTransformer>()),
        time_controller_(Timestamp::Seconds(0)) {}

  ~RtpSenderVideoFrameTransformerDelegateTest() override = default;

  std::unique_ptr<TransformableFrameInterface> GetTransformableFrame(
      rtc::scoped_refptr<RTPSenderVideoFrameTransformerDelegate> delegate,
      bool key_frame = false) {
    EncodedImage encoded_image;
    encoded_image.SetEncodedData(EncodedImageBuffer::Create(1));
    encoded_image._frameType = key_frame ? VideoFrameType::kVideoFrameKey
                                         : VideoFrameType::kVideoFrameDelta;
    std::unique_ptr<TransformableFrameInterface> frame = nullptr;
    EXPECT_CALL(*frame_transformer_, Transform)
        .WillOnce([&](std::unique_ptr<TransformableFrameInterface>
                          frame_to_transform) {
          frame = std::move(frame_to_transform);
        });
    delegate->TransformFrame(
        /*payload_type=*/1, VideoCodecType::kVideoCodecVP8, /*rtp_timestamp=*/2,
        encoded_image, RTPVideoHeader(),
        /*expected_retransmission_time_ms=*/absl::nullopt);
    return frame;
  }

  MockRTPVideoFrameSenderInterface test_sender_;
  rtc::scoped_refptr<MockFrameTransformer> frame_transformer_;
  GlobalSimulatedTimeController time_controller_;
};

TEST_F(RtpSenderVideoFrameTransformerDelegateTest,
       RegisterTransformedFrameCallbackSinkOnInit) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, /*csrcs=*/std::vector<uint32_t>(),
      time_controller_.CreateTaskQueueFactory().get());
  EXPECT_CALL(*frame_transformer_,
              RegisterTransformedFrameSinkCallback(_, 1111));
  delegate->Init();
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest,
       UnregisterTransformedFrameSinkCallbackOnReset) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, /*csrcs=*/std::vector<uint32_t>(),
      time_controller_.CreateTaskQueueFactory().get());
  EXPECT_CALL(*frame_transformer_,
              UnregisterTransformedFrameSinkCallback(1111));
  delegate->Reset();
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest,
       TransformFrameCallsTransform) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, /*csrcs=*/std::vector<uint32_t>(),
      time_controller_.CreateTaskQueueFactory().get());

  EncodedImage encoded_image;
  EXPECT_CALL(*frame_transformer_, Transform);
  delegate->TransformFrame(
      /*payload_type=*/1, VideoCodecType::kVideoCodecVP8, /*rtp_timestamp=*/2,
      encoded_image, RTPVideoHeader(),
      /*expected_retransmission_time_ms=*/absl::nullopt);
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest,
       OnTransformedFrameCallsSenderSendVideo) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, /*csrcs=*/std::vector<uint32_t>(),
      time_controller_.CreateTaskQueueFactory().get());

  rtc::scoped_refptr<TransformedFrameCallback> callback;
  EXPECT_CALL(*frame_transformer_, RegisterTransformedFrameSinkCallback)
      .WillOnce(SaveArg<0>(&callback));
  delegate->Init();
  ASSERT_TRUE(callback);

  std::unique_ptr<TransformableFrameInterface> frame =
      GetTransformableFrame(delegate);
  ASSERT_TRUE(frame);

  rtc::Event event;
  EXPECT_CALL(test_sender_, SendVideo).WillOnce(WithoutArgs([&] {
    event.Set();
    return true;
  }));

  callback->OnTransformedFrame(std::move(frame));

  event.Wait(TimeDelta::Seconds(1));
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest, CloneSenderVideoFrame) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, /*csrcs=*/std::vector<uint32_t>(),
      time_controller_.CreateTaskQueueFactory().get());

  std::unique_ptr<TransformableFrameInterface> frame =
      GetTransformableFrame(delegate);
  ASSERT_TRUE(frame);

  auto& video_frame = static_cast<TransformableVideoFrameInterface&>(*frame);
  std::unique_ptr<TransformableVideoFrameInterface> clone =
      CloneSenderVideoFrame(&video_frame);

  EXPECT_EQ(clone->IsKeyFrame(), video_frame.IsKeyFrame());
  EXPECT_EQ(clone->GetPayloadType(), video_frame.GetPayloadType());
  EXPECT_EQ(clone->GetSsrc(), video_frame.GetSsrc());
  EXPECT_EQ(clone->GetTimestamp(), video_frame.GetTimestamp());
  EXPECT_EQ(clone->Metadata(), video_frame.Metadata());
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest, CloneKeyFrame) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, /*csrcs=*/std::vector<uint32_t>(),
      time_controller_.CreateTaskQueueFactory().get());

  std::unique_ptr<TransformableFrameInterface> frame =
      GetTransformableFrame(delegate, /*key_frame=*/true);
  ASSERT_TRUE(frame);

  auto& video_frame = static_cast<TransformableVideoFrameInterface&>(*frame);
  std::unique_ptr<TransformableVideoFrameInterface> clone =
      CloneSenderVideoFrame(&video_frame);

  EXPECT_EQ(clone->IsKeyFrame(), video_frame.IsKeyFrame());
  EXPECT_EQ(clone->GetPayloadType(), video_frame.GetPayloadType());
  EXPECT_EQ(clone->GetSsrc(), video_frame.GetSsrc());
  EXPECT_EQ(clone->GetTimestamp(), video_frame.GetTimestamp());
  EXPECT_EQ(clone->Metadata(), video_frame.Metadata());
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest, MetadataAfterSetMetadata) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, /*csrcs=*/std::vector<uint32_t>(),
      time_controller_.CreateTaskQueueFactory().get());

  std::unique_ptr<TransformableFrameInterface> frame =
      GetTransformableFrame(delegate);
  ASSERT_TRUE(frame);
  auto& video_frame = static_cast<TransformableVideoFrameInterface&>(*frame);

  VideoFrameMetadata metadata;
  metadata.SetFrameType(VideoFrameType::kVideoFrameKey);
  metadata.SetFrameId(654);
  metadata.SetSsrc(2222);
  metadata.SetCsrcs({1, 2, 3});

  video_frame.SetMetadata(metadata);
  VideoFrameMetadata actual_metadata = video_frame.Metadata();

  // TODO(bugs.webrtc.org/14708): Just EXPECT_EQ the whole Metadata once the
  // equality operator lands.
  EXPECT_EQ(metadata.GetFrameType(), actual_metadata.GetFrameType());
  EXPECT_EQ(metadata.GetFrameId(), actual_metadata.GetFrameId());
  EXPECT_EQ(metadata.GetSsrc(), actual_metadata.GetSsrc());
  EXPECT_EQ(metadata.GetCsrcs(), actual_metadata.GetCsrcs());
}

}  // namespace
}  // namespace webrtc
