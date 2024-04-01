// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cstdlib>
#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/sender/sender_encoded_frame.h"
#include "media/cast/sender/vpx_encoder.h"
#include "media/cast/sender/vpx_quantizer_parser.h"
#include "media/cast/test/receiver/video_decoder.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/video_utility.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

namespace {
const int kWidth = 32;
const int kHeight = 32;
const int kFrameRate = 10;
const int kQp = 20;

FrameSenderConfig GetVideoConfigForTest() {
  FrameSenderConfig config = GetDefaultVideoSenderConfig();
  config.codec = CODEC_VIDEO_VP8;
  config.use_external_encoder = false;
  config.max_frame_rate = kFrameRate;
  config.video_codec_params.min_qp = kQp;
  config.video_codec_params.max_qp = kQp;
  config.video_codec_params.max_cpu_saver_qp = kQp;
  return config;
}
}  // unnamed namespace

class VpxQuantizerParserTest : public ::testing::Test {
 public:
  VpxQuantizerParserTest() : video_config_(GetVideoConfigForTest()) {}

  // Call vp8 software encoder to encode one randomly generated frame.
  void EncodeOneFrame(SenderEncodedFrame* encoded_frame) {
    const gfx::Size frame_size = gfx::Size(kWidth, kHeight);
    const scoped_refptr<VideoFrame> video_frame = VideoFrame::CreateFrame(
        PIXEL_FORMAT_I420, frame_size, gfx::Rect(frame_size), frame_size,
        next_frame_timestamp_);
    const base::TimeTicks reference_time =
        base::TimeTicks::UnixEpoch() + next_frame_timestamp_;
    next_frame_timestamp_ += base::Seconds(1) / kFrameRate;
    PopulateVideoFrameWithNoise(video_frame.get());
    vp8_encoder_->Encode(video_frame, reference_time, encoded_frame);
  }

  // Update the vp8 encoder with the new quantizer.
  void UpdateQuantizer(int qp) {
    DCHECK((qp > 3) && (qp < 64));
    video_config_.video_codec_params.min_qp = qp;
    video_config_.video_codec_params.max_qp = qp;
    video_config_.video_codec_params.max_cpu_saver_qp = qp;
    RecreateVp8Encoder();
  }

 protected:
  void SetUp() final {
    next_frame_timestamp_ = base::TimeDelta();
    RecreateVp8Encoder();
  }

 private:
  // Reconstruct a vp8 encoder with new config since the Vp8Encoder
  // class has no interface to update the config.
  void RecreateVp8Encoder() {
    vp8_encoder_ = std::make_unique<VpxEncoder>(video_config_);
    vp8_encoder_->Initialize();
  }

  base::TimeDelta next_frame_timestamp_;
  FrameSenderConfig video_config_;
  std::unique_ptr<VpxEncoder> vp8_encoder_;
};

// Encode 3 frames to test the cases with insufficient data input.
TEST_F(VpxQuantizerParserTest, InsufficientData) {
  for (int i = 0; i < 3; ++i) {
    std::unique_ptr<SenderEncodedFrame> encoded_frame(new SenderEncodedFrame());
    const uint8_t* encoded_data =
        reinterpret_cast<const uint8_t*>(encoded_frame->data.data());
    // Null input.
    int decoded_quantizer =
        ParseVpxHeaderQuantizer(encoded_data, encoded_frame->data.size());
    EXPECT_EQ(-1, decoded_quantizer);
    EncodeOneFrame(encoded_frame.get());
    encoded_data = reinterpret_cast<const uint8_t*>(encoded_frame->data.data());
    // Zero bytes should not be enough to decode the quantizer value.
    decoded_quantizer = ParseVpxHeaderQuantizer(encoded_data, 0);
    EXPECT_EQ(-1, decoded_quantizer);
    // Three bytes should not be enough to decode the quantizer value..
    decoded_quantizer = ParseVpxHeaderQuantizer(encoded_data, 3);
    EXPECT_EQ(-1, decoded_quantizer);
    unsigned int first_partition_size =
        (encoded_data[0] | (encoded_data[1] << 8) | (encoded_data[2] << 16)) >>
        5;
    if (encoded_frame->dependency == EncodedFrame::KEY) {
      // Ten bytes should not be enough to decode the quanitizer value
      // for a Key frame.
      decoded_quantizer = ParseVpxHeaderQuantizer(encoded_data, 10);
      EXPECT_EQ(-1, decoded_quantizer);
      // One byte less than needed to decode the quantizer value.
      decoded_quantizer =
          ParseVpxHeaderQuantizer(encoded_data, 10 + first_partition_size - 1);
      EXPECT_EQ(-1, decoded_quantizer);
      // Minimum number of bytes to decode the quantizer value.
      decoded_quantizer =
          ParseVpxHeaderQuantizer(encoded_data, 10 + first_partition_size);
      EXPECT_EQ(kQp, decoded_quantizer);
    } else {
      // One byte less than needed to decode the quantizer value.
      decoded_quantizer =
          ParseVpxHeaderQuantizer(encoded_data, 3 + first_partition_size - 1);
      EXPECT_EQ(-1, decoded_quantizer);
      // Minimum number of bytes to decode the quantizer value.
      decoded_quantizer =
          ParseVpxHeaderQuantizer(encoded_data, 3 + first_partition_size);
      EXPECT_EQ(kQp, decoded_quantizer);
    }
  }
}

// Encode 3 fames for every quantizer value in the range of [4,63].
TEST_F(VpxQuantizerParserTest, VariedQuantizer) {
  int decoded_quantizer = -1;
  for (int qp = 4; qp <= 63; qp += 10) {
    UpdateQuantizer(qp);
    for (int i = 0; i < 3; ++i) {
      std::unique_ptr<SenderEncodedFrame> encoded_frame(
          new SenderEncodedFrame());
      EncodeOneFrame(encoded_frame.get());
      decoded_quantizer = ParseVpxHeaderQuantizer(
          reinterpret_cast<const uint8_t*>(encoded_frame->data.data()),
          encoded_frame->data.size());
      EXPECT_EQ(qp, decoded_quantizer);
    }
  }
}

}  // namespace cast
}  // namespace media
