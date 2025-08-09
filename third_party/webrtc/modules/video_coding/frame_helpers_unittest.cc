/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/frame_helpers.h"

#include <utility>

#include "api/scoped_refptr.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_frame.h"
#include "api/video/encoded_image.h"
#include "common_video/frame_instrumentation_data.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;

constexpr uint32_t kRtpTimestamp = 123456710;

webrtc::scoped_refptr<EncodedImageBuffer> CreateEncodedImageBufferOfSizeN(
    size_t n,
    uint8_t x) {
  webrtc::scoped_refptr<EncodedImageBuffer> buffer =
      EncodedImageBuffer::Create(n);
  for (size_t i = 0; i < n; ++i) {
    buffer->data()[i] = static_cast<uint8_t>(x + i);
  }
  return buffer;
}

// Returns an `EncodedFrame` with data values [x, x+1, ... x+(n-1)].
EncodedFrame CreateEncodedImageOfSizeN(size_t n, uint8_t x) {
  EncodedFrame image;
  image.SetEncodedData(CreateEncodedImageBufferOfSizeN(n, x));
  image.SetRtpTimestamp(kRtpTimestamp);
  return image;
}

TEST(FrameHasBadRenderTimingTest, LargePositiveFrameDelayIsBad) {
  Timestamp render_time = Timestamp::Seconds(12);
  Timestamp now = Timestamp::Seconds(0);

  EXPECT_TRUE(FrameHasBadRenderTiming(render_time, now));
}

TEST(FrameHasBadRenderTimingTest, LargeNegativeFrameDelayIsBad) {
  Timestamp render_time = Timestamp::Seconds(12);
  Timestamp now = Timestamp::Seconds(24);

  EXPECT_TRUE(FrameHasBadRenderTiming(render_time, now));
}

TEST(FrameInstrumentationDataTest,
     CombinedFrameHasSameDataAsHighestSpatialLayer) {
  // Assume L2T1 scalability mode.
  EncodedFrame spatial_layer_1 = CreateEncodedImageOfSizeN(/*n=*/10, /*x=*/1);
  const FrameInstrumentationData frame_ins_data_1 = {
      .sequence_index = 100,
      .communicate_upper_bits = false,
      .std_dev = 0.5,
      .luma_error_threshold = 5,
      .chroma_error_threshold = 4,
      .sample_values = {0.2, 0.7, 1.9}};
  spatial_layer_1.SetFrameInstrumentationData(frame_ins_data_1);

  EncodedFrame spatial_layer_2 = CreateEncodedImageOfSizeN(/*n=*/10, /*x=*/11);
  FrameInstrumentationData frame_ins_data_2 = {
      .sequence_index = 10,
      .communicate_upper_bits = false,
      .std_dev = 1.0,
      .luma_error_threshold = 3,
      .chroma_error_threshold = 4,
      .sample_values = {0.1, 0.3, 2.1}};
  spatial_layer_2.SetFrameInstrumentationData(frame_ins_data_2);

  absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4> frames;
  frames.push_back(std::make_unique<EncodedFrame>(spatial_layer_1));
  frames.push_back(std::make_unique<EncodedFrame>(spatial_layer_2));

  std::optional<
      absl::variant<FrameInstrumentationSyncData, FrameInstrumentationData>>
      data = CombineAndDeleteFrames(std::move(frames))
                 ->CodecSpecific()
                 ->frame_instrumentation_data;

  ASSERT_TRUE(data.has_value());
  ASSERT_TRUE(absl::holds_alternative<FrameInstrumentationData>(*data));
  FrameInstrumentationData frame_instrumentation_data =
      absl::get<FrameInstrumentationData>(*data);

  // Expect to have the same frame_instrumentation_data as the highest spatial
  // layer.
  EXPECT_EQ(frame_instrumentation_data.sequence_index, 10);
  EXPECT_FALSE(frame_instrumentation_data.communicate_upper_bits);
  EXPECT_EQ(frame_instrumentation_data.std_dev, 1.0);
  EXPECT_EQ(frame_instrumentation_data.luma_error_threshold, 3);
  EXPECT_EQ(frame_instrumentation_data.chroma_error_threshold, 4);
  EXPECT_THAT(frame_instrumentation_data.sample_values,
              ElementsAre(0.1, 0.3, 2.1));
}

}  // namespace
}  // namespace webrtc
