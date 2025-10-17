/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/channel_receive_frame_transformer_delegate.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "api/array_view.h"
#include "api/frame_transformer_factory.h"
#include "api/frame_transformer_interface.h"
#include "api/make_ref_counted.h"
#include "api/rtp_headers.h"
#include "api/scoped_refptr.h"
#include "api/test/mock_frame_transformer.h"
#include "api/test/mock_transformable_audio_frame.h"
#include "api/units/timestamp.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/ntp_time.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

constexpr Timestamp kFakeReceiveTimestamp = Timestamp::Millis(1234567);

class MockChannelReceive {
 public:
  MOCK_METHOD(void,
              ReceiveFrame,
              (ArrayView<const uint8_t> packet,
               const RTPHeader& header,
               Timestamp receive_time));

  ChannelReceiveFrameTransformerDelegate::ReceiveFrameCallback callback() {
    return [this](ArrayView<const uint8_t> packet, const RTPHeader& header,
                  Timestamp receive_time) {
      ReceiveFrame(packet, header, receive_time);
    };
  }
};

// Test that the delegate registers itself with the frame transformer on Init().
TEST(ChannelReceiveFrameTransformerDelegateTest,
     RegisterTransformedFrameCallbackOnInit) {
  scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      make_ref_counted<MockFrameTransformer>();
  scoped_refptr<ChannelReceiveFrameTransformerDelegate> delegate =
      make_ref_counted<ChannelReceiveFrameTransformerDelegate>(
          ChannelReceiveFrameTransformerDelegate::ReceiveFrameCallback(),
          mock_frame_transformer, nullptr);
  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameCallback);
  delegate->Init();
}

// Test that the delegate unregisters itself from the frame transformer on
// Reset().
TEST(ChannelReceiveFrameTransformerDelegateTest,
     UnregisterTransformedFrameCallbackOnReset) {
  scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      make_ref_counted<MockFrameTransformer>();
  scoped_refptr<ChannelReceiveFrameTransformerDelegate> delegate =
      make_ref_counted<ChannelReceiveFrameTransformerDelegate>(
          ChannelReceiveFrameTransformerDelegate::ReceiveFrameCallback(),
          mock_frame_transformer, nullptr);
  EXPECT_CALL(*mock_frame_transformer, UnregisterTransformedFrameCallback);
  delegate->Reset();
}

// Test that when the delegate receives a transformed frame from the frame
// transformer, it passes it to the channel using the ReceiveFrameCallback.
TEST(ChannelReceiveFrameTransformerDelegateTest,
     TransformRunsChannelReceiveCallback) {
  AutoThread main_thread;
  scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      make_ref_counted<NiceMock<MockFrameTransformer>>();
  MockChannelReceive mock_channel;
  scoped_refptr<ChannelReceiveFrameTransformerDelegate> delegate =
      make_ref_counted<ChannelReceiveFrameTransformerDelegate>(
          mock_channel.callback(), mock_frame_transformer, Thread::Current());
  scoped_refptr<TransformedFrameCallback> callback;
  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameCallback)
      .WillOnce(SaveArg<0>(&callback));
  delegate->Init();
  ASSERT_TRUE(callback);

  const uint8_t data[] = {1, 2, 3, 4};
  ArrayView<const uint8_t> packet(data, sizeof(data));
  RTPHeader header;
  EXPECT_CALL(mock_channel, ReceiveFrame);
  ON_CALL(*mock_frame_transformer, Transform)
      .WillByDefault(
          [&callback](std::unique_ptr<TransformableFrameInterface> frame) {
            callback->OnTransformedFrame(std::move(frame));
          });
  delegate->Transform(packet, header, /*ssrc=*/1111, /*mimeType=*/"audio/opus",
                      kFakeReceiveTimestamp);
  ThreadManager::ProcessAllMessageQueuesForTesting();
}

// Test that when the delegate receives a Outgoing frame from the frame
// transformer, it passes it to the channel using the ReceiveFrameCallback.
TEST(ChannelReceiveFrameTransformerDelegateTest,
     TransformRunsChannelReceiveCallbackForSenderFrame) {
  AutoThread main_thread;
  scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      make_ref_counted<NiceMock<MockFrameTransformer>>();
  MockChannelReceive mock_channel;
  scoped_refptr<ChannelReceiveFrameTransformerDelegate> delegate =
      make_ref_counted<ChannelReceiveFrameTransformerDelegate>(
          mock_channel.callback(), mock_frame_transformer, Thread::Current());
  scoped_refptr<TransformedFrameCallback> callback;
  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameCallback)
      .WillOnce(SaveArg<0>(&callback));
  delegate->Init();
  ASSERT_TRUE(callback);

  const uint8_t data[] = {1, 2, 3, 4};
  ArrayView<const uint8_t> packet(data, sizeof(data));
  RTPHeader header;
  EXPECT_CALL(mock_channel,
              ReceiveFrame(ElementsAre(1, 2, 3, 4), _, kFakeReceiveTimestamp));
  ON_CALL(*mock_frame_transformer, Transform)
      .WillByDefault(
          [&callback](std::unique_ptr<TransformableFrameInterface> frame) {
            auto* transformed_frame =
                static_cast<TransformableAudioFrameInterface*>(frame.get());
            callback->OnTransformedFrame(CloneAudioFrame(transformed_frame));
          });
  delegate->Transform(packet, header, /*ssrc=*/1111, /*mimeType=*/"audio/opus",
                      kFakeReceiveTimestamp);
  ThreadManager::ProcessAllMessageQueuesForTesting();
}

// Test that if the delegate receives a transformed frame after it has been
// reset, it does not run the ReceiveFrameCallback, as the channel is destroyed
// after resetting the delegate.
TEST(ChannelReceiveFrameTransformerDelegateTest,
     OnTransformedDoesNotRunChannelReceiveCallbackAfterReset) {
  AutoThread main_thread;
  scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      make_ref_counted<testing::NiceMock<MockFrameTransformer>>();
  MockChannelReceive mock_channel;
  scoped_refptr<ChannelReceiveFrameTransformerDelegate> delegate =
      make_ref_counted<ChannelReceiveFrameTransformerDelegate>(
          mock_channel.callback(), mock_frame_transformer, Thread::Current());

  delegate->Reset();
  EXPECT_CALL(mock_channel, ReceiveFrame).Times(0);
  delegate->OnTransformedFrame(std::make_unique<MockTransformableAudioFrame>());
  ThreadManager::ProcessAllMessageQueuesForTesting();
}

TEST(ChannelReceiveFrameTransformerDelegateTest,
     ShortCircuitingSkipsTransform) {
  AutoThread main_thread;
  scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      make_ref_counted<testing::NiceMock<MockFrameTransformer>>();
  MockChannelReceive mock_channel;
  scoped_refptr<ChannelReceiveFrameTransformerDelegate> delegate =
      make_ref_counted<ChannelReceiveFrameTransformerDelegate>(
          mock_channel.callback(), mock_frame_transformer, Thread::Current());
  const uint8_t data[] = {1, 2, 3, 4};
  ArrayView<const uint8_t> packet(data, sizeof(data));
  RTPHeader header;

  delegate->StartShortCircuiting();
  ThreadManager::ProcessAllMessageQueuesForTesting();

  // Will not call the actual transformer.
  EXPECT_CALL(*mock_frame_transformer, Transform).Times(0);
  // Will pass the frame straight to the channel.
  EXPECT_CALL(mock_channel, ReceiveFrame);
  delegate->Transform(packet, header, /*ssrc=*/1111, /*mimeType=*/"audio/opus",
                      kFakeReceiveTimestamp);
}

TEST(ChannelReceiveFrameTransformerDelegateTest,
     AudioLevelAndCaptureTimeAbsentWithoutExtension) {
  AutoThread main_thread;
  scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      make_ref_counted<NiceMock<MockFrameTransformer>>();
  scoped_refptr<ChannelReceiveFrameTransformerDelegate> delegate =
      make_ref_counted<ChannelReceiveFrameTransformerDelegate>(
          /*receive_frame_callback=*/nullptr, mock_frame_transformer,
          Thread::Current());
  scoped_refptr<TransformedFrameCallback> callback;
  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameCallback)
      .WillOnce(SaveArg<0>(&callback));
  delegate->Init();
  ASSERT_TRUE(callback);

  const uint8_t data[] = {1, 2, 3, 4};
  ArrayView<const uint8_t> packet(data, sizeof(data));
  RTPHeader header;
  std::unique_ptr<TransformableFrameInterface> frame;
  ON_CALL(*mock_frame_transformer, Transform)
      .WillByDefault(
          [&](std::unique_ptr<TransformableFrameInterface> transform_frame) {
            frame = std::move(transform_frame);
          });
  delegate->Transform(packet, header, /*ssrc=*/1111, /*mimeType=*/"audio/opus",
                      kFakeReceiveTimestamp);

  EXPECT_TRUE(frame);
  auto* audio_frame =
      static_cast<TransformableAudioFrameInterface*>(frame.get());
  EXPECT_FALSE(audio_frame->AudioLevel());
  EXPECT_FALSE(audio_frame->CaptureTime());
  EXPECT_FALSE(audio_frame->SenderCaptureTimeOffset());
  EXPECT_EQ(audio_frame->Type(),
            TransformableAudioFrameInterface::FrameType::kAudioFrameCN);
}

TEST(ChannelReceiveFrameTransformerDelegateTest,
     AudioLevelPresentWithExtension) {
  AutoThread main_thread;
  scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      make_ref_counted<NiceMock<MockFrameTransformer>>();
  scoped_refptr<ChannelReceiveFrameTransformerDelegate> delegate =
      make_ref_counted<ChannelReceiveFrameTransformerDelegate>(
          /*receive_frame_callback=*/nullptr, mock_frame_transformer,
          Thread::Current());
  scoped_refptr<TransformedFrameCallback> callback;
  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameCallback)
      .WillOnce(SaveArg<0>(&callback));
  delegate->Init();
  ASSERT_TRUE(callback);

  const uint8_t data[] = {1, 2, 3, 4};
  ArrayView<const uint8_t> packet(data, sizeof(data));
  RTPHeader header;
  uint8_t audio_level_dbov = 67;
  AudioLevel audio_level(/*voice_activity=*/true, audio_level_dbov);
  header.extension.set_audio_level(audio_level);
  std::unique_ptr<TransformableFrameInterface> frame;
  ON_CALL(*mock_frame_transformer, Transform)
      .WillByDefault(
          [&](std::unique_ptr<TransformableFrameInterface> transform_frame) {
            frame = std::move(transform_frame);
          });
  delegate->Transform(packet, header, /*ssrc=*/1111, /*mimeType=*/"audio/opus",
                      kFakeReceiveTimestamp);

  EXPECT_TRUE(frame);
  auto* audio_frame =
      static_cast<TransformableAudioFrameInterface*>(frame.get());
  EXPECT_EQ(*audio_frame->AudioLevel(), audio_level_dbov);
  EXPECT_EQ(audio_frame->Type(),
            TransformableAudioFrameInterface::FrameType::kAudioFrameSpeech);
}

TEST(ChannelReceiveFrameTransformerDelegateTest,
     CaptureTimePresentWithExtension) {
  AutoThread main_thread;
  scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      make_ref_counted<NiceMock<MockFrameTransformer>>();
  scoped_refptr<ChannelReceiveFrameTransformerDelegate> delegate =
      make_ref_counted<ChannelReceiveFrameTransformerDelegate>(
          /*receive_frame_callback=*/nullptr, mock_frame_transformer,
          Thread::Current());
  scoped_refptr<TransformedFrameCallback> callback;
  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameCallback)
      .WillOnce(SaveArg<0>(&callback));
  delegate->Init();
  ASSERT_TRUE(callback);

  const uint8_t data[] = {1, 2, 3, 4};
  ArrayView<const uint8_t> packet(data, sizeof(data));
  Timestamp capture_time = Timestamp::Millis(1234);
  TimeDelta sender_capture_time_offsets[] = {TimeDelta::Millis(56),
                                             TimeDelta::Millis(-79)};
  for (auto offset : sender_capture_time_offsets) {
    AbsoluteCaptureTime absolute_capture_time = {
        .absolute_capture_timestamp = Int64MsToUQ32x32(capture_time.ms()),
        .estimated_capture_clock_offset = Int64MsToQ32x32(offset.ms())};
    RTPHeader header;
    header.extension.absolute_capture_time = absolute_capture_time;

    std::unique_ptr<TransformableFrameInterface> frame;
    ON_CALL(*mock_frame_transformer, Transform)
        .WillByDefault(
            [&](std::unique_ptr<TransformableFrameInterface> transform_frame) {
              frame = std::move(transform_frame);
            });
    delegate->Transform(packet, header, /*ssrc=*/1111,
                        /*mimeType=*/"audio/opus", kFakeReceiveTimestamp);

    EXPECT_TRUE(frame);
    auto* audio_frame =
        static_cast<TransformableAudioFrameInterface*>(frame.get());
    EXPECT_EQ(*audio_frame->CaptureTime(), capture_time);
    EXPECT_EQ(*audio_frame->SenderCaptureTimeOffset(), offset);
  }
}

TEST(ChannelReceiveFrameTransformerDelegateTest, SetAudioLevel) {
  AutoThread main_thread;
  scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      make_ref_counted<NiceMock<MockFrameTransformer>>();
  scoped_refptr<ChannelReceiveFrameTransformerDelegate> delegate =
      make_ref_counted<ChannelReceiveFrameTransformerDelegate>(
          /*receive_frame_callback=*/nullptr, mock_frame_transformer,
          Thread::Current());
  delegate->Init();
  const uint8_t data[] = {1, 2, 3, 4};
  ArrayView<const uint8_t> packet(data, sizeof(data));
  std::unique_ptr<TransformableFrameInterface> frame;
  ON_CALL(*mock_frame_transformer, Transform)
      .WillByDefault(
          [&](std::unique_ptr<TransformableFrameInterface> transform_frame) {
            frame = std::move(transform_frame);
          });
  delegate->Transform(packet, RTPHeader(), /*ssrc=*/1111,
                      /*mimeType=*/"audio/opus", kFakeReceiveTimestamp);

  EXPECT_TRUE(frame);
  auto* audio_frame =
      static_cast<TransformableAudioFrameInterface*>(frame.get());
  EXPECT_TRUE(audio_frame->CanSetAudioLevel());
  EXPECT_FALSE(audio_frame->AudioLevel().has_value());

  audio_frame->SetAudioLevel(67u);
  EXPECT_EQ(audio_frame->AudioLevel(), 67u);

  // Audio level is clamped to the range [0, 127].
  audio_frame->SetAudioLevel(128u);
  EXPECT_EQ(audio_frame->AudioLevel(), 127u);
}

TEST(ChannelReceiveFrameTransformerDelegateTest,
     ReceivingSenderFrameWithAudioValueSetsAudioLevelInHeader) {
  AutoThread main_thread;
  scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      make_ref_counted<NiceMock<MockFrameTransformer>>();
  MockChannelReceive mock_channel;
  scoped_refptr<ChannelReceiveFrameTransformerDelegate> delegate =
      make_ref_counted<ChannelReceiveFrameTransformerDelegate>(
          mock_channel.callback(), mock_frame_transformer, Thread::Current());
  delegate->Init();

  std::unique_ptr<MockTransformableAudioFrame> audio_frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  ON_CALL(*audio_frame, GetDirection())
      .WillByDefault(Return(TransformableFrameInterface::Direction::kSender));
  ON_CALL(*audio_frame, AudioLevel()).WillByDefault(Return(111u));

  RTPHeader header;
  EXPECT_CALL(mock_channel, ReceiveFrame).WillOnce(SaveArg<1>(&header));
  delegate->OnTransformedFrame(std::move(audio_frame));
  ThreadManager::ProcessAllMessageQueuesForTesting();

  ASSERT_TRUE(header.extension.audio_level().has_value());
  EXPECT_EQ(header.extension.audio_level()->level(), 111);
}

}  // namespace
}  // namespace webrtc
