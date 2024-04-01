// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_encode_accelerator_adapter.h"

#include <memory>
#include <string>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/mock_media_log.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"

#if BUILDFLAG(ENABLE_OPENH264)
#include "media/video/openh264_video_encoder.h"
#endif

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
#include "media/filters/ffmpeg_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_LIBVPX)
#include "media/filters/vpx_video_decoder.h"
#include "media/video/vpx_video_encoder.h"
#endif

namespace media {

struct SwVideoTestParams {
  VideoCodec codec;
  VideoCodecProfile profile;
  VideoPixelFormat pixel_format;
  absl::optional<SVCScalabilityMode> scalability_mode;
};

class SoftwareVideoEncoderTest
    : public ::testing::TestWithParam<SwVideoTestParams> {
 public:
  SoftwareVideoEncoderTest() = default;

  void SetUp() override {
    auto args = GetParam();
    profile_ = args.profile;
    pixel_format_ = args.pixel_format;
    codec_ = args.codec;
    encoder_ = CreateEncoder(codec_);
  }

  void TearDown() override {
    encoder_.reset();
    decoder_.reset();
    RunUntilIdle();
  }

  void PrepareDecoder(
      gfx::Size size,
      VideoDecoder::OutputCB output_cb,
      std::vector<uint8_t> extra_data = std::vector<uint8_t>()) {
    gfx::Rect visible_rect(size.width(), size.height());
    VideoDecoderConfig config(
        codec_, profile_, VideoDecoderConfig::AlphaMode::kIsOpaque,
        VideoColorSpace::JPEG(), VideoTransformation(), size, visible_rect,
        size, extra_data, EncryptionScheme::kUnencrypted);

    if (codec_ == VideoCodec::kH264 || codec_ == VideoCodec::kVP8) {
#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
      decoder_ = std::make_unique<FFmpegVideoDecoder>(&media_log_);
#endif
    } else if (codec_ == VideoCodec::kVP9) {
#if BUILDFLAG(ENABLE_LIBVPX)
      decoder_ = std::make_unique<VpxVideoDecoder>();
#endif
    }

    EXPECT_NE(decoder_, nullptr);
    decoder_->Initialize(config, false, nullptr, ValidatingStatusCB(),
                         std::move(output_cb), base::NullCallback());
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  scoped_refptr<VideoFrame> CreateI420Frame(gfx::Size size,
                                            uint32_t color,
                                            base::TimeDelta timestamp) {
    auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I420, size,
                                         gfx::Rect(size), size, timestamp);
    auto y = color & 0xFF;
    auto u = (color >> 8) & 0xFF;
    auto v = (color >> 16) & 0xFF;
    libyuv::I420Rect(
        frame->data(VideoFrame::kYPlane), frame->stride(VideoFrame::kYPlane),
        frame->data(VideoFrame::kUPlane), frame->stride(VideoFrame::kUPlane),
        frame->data(VideoFrame::kVPlane), frame->stride(VideoFrame::kVPlane),
        0,                               // left
        0,                               // top
        frame->visible_rect().width(),   // right
        frame->visible_rect().height(),  // bottom
        y,                               // Y color
        u,                               // U color
        v);                              // V color
    return frame;
  }

  scoped_refptr<VideoFrame> CreateRGBFrame(gfx::Size size,
                                           uint32_t color,
                                           base::TimeDelta timestamp) {
    auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_XRGB, size,
                                         gfx::Rect(size), size, timestamp);

    libyuv::ARGBRect(frame->data(VideoFrame::kARGBPlane),
                     frame->stride(VideoFrame::kARGBPlane),
                     0,                               // left
                     0,                               // top
                     frame->visible_rect().width(),   // right
                     frame->visible_rect().height(),  // bottom
                     color);

    return frame;
  }

  scoped_refptr<VideoFrame> CreateFrame(gfx::Size size,
                                        VideoPixelFormat format,
                                        base::TimeDelta timestamp,
                                        uint32_t color = 0x964050) {
    switch (format) {
      case PIXEL_FORMAT_I420:
        return CreateI420Frame(size, color, timestamp);
      case PIXEL_FORMAT_XRGB:
        return CreateRGBFrame(size, color, timestamp);
      default:
        EXPECT_TRUE(false) << "not supported pixel format";
        return nullptr;
    }
  }

  std::unique_ptr<VideoEncoder> CreateEncoder(VideoCodec codec) {
    switch (codec) {
      case media::VideoCodec::kVP8:
      case media::VideoCodec::kVP9:
#if BUILDFLAG(ENABLE_LIBVPX)
        return std::make_unique<media::VpxVideoEncoder>();
#else
        return nullptr;
#endif
      case media::VideoCodec::kH264:
#if BUILDFLAG(ENABLE_OPENH264)
        return std::make_unique<OpenH264VideoEncoder>();
#else
        return nullptr;
#endif
      default:
        return nullptr;
    }
  }

  VideoEncoder::StatusCB ValidatingStatusCB(base::Location loc = FROM_HERE) {
    struct CallEnforcer {
      bool called = false;
      std::string location;
      ~CallEnforcer() {
        EXPECT_TRUE(called) << "Callback created: " << location;
      }
    };
    auto enforcer = std::make_unique<CallEnforcer>();
    enforcer->location = loc.ToString();
    return base::BindLambdaForTesting(
        [enforcer{std::move(enforcer)}](Status s) {
          EXPECT_TRUE(s.is_ok())
              << " Callback created: " << enforcer->location
              << " Code: " << s.code() << " Error: " << s.message();
          enforcer->called = true;
        });
  }

  void DecodeAndWaitForStatus(
      scoped_refptr<DecoderBuffer> buffer,
      const base::Location& location = base::Location::Current()) {
    base::RunLoop run_loop;
    decoder_->Decode(
        std::move(buffer), base::BindLambdaForTesting([&](Status status) {
          EXPECT_TRUE(status.is_ok())
              << " Callback created: " << location.ToString()
              << " Code: " << status.code() << " Error: " << status.message();
          run_loop.Quit();
        }));
    run_loop.Run(location);
  }

  int CountDifferentPixels(VideoFrame& frame1, VideoFrame& frame2) {
    int diff_cnt = 0;
    uint8_t tolerance = 10;

    if (frame1.format() != frame2.format() ||
        frame1.visible_rect() != frame2.visible_rect()) {
      return frame1.coded_size().GetArea();
    }

    VideoPixelFormat format = frame1.format();
    size_t num_planes = VideoFrame::NumPlanes(format);
    gfx::Size visible_size = frame1.visible_rect().size();
    for (size_t plane = 0; plane < num_planes; ++plane) {
      uint8_t* data1 = frame1.visible_data(plane);
      int stride1 = frame1.stride(plane);
      uint8_t* data2 = frame2.visible_data(plane);
      int stride2 = frame2.stride(plane);
      size_t rows = VideoFrame::Rows(plane, format, visible_size.height());
      int row_bytes = VideoFrame::RowBytes(plane, format, visible_size.width());

      for (size_t r = 0; r < rows; ++r) {
        for (int c = 0; c < row_bytes; ++c) {
          uint8_t b1 = data1[(stride1 * r) + c];
          uint8_t b2 = data2[(stride2 * r) + c];
          uint8_t diff = std::max(b1, b2) - std::min(b1, b2);
          if (diff > tolerance)
            diff_cnt++;
        }
      }
    }
    return diff_cnt;
  }

 protected:
  VideoCodec codec_;
  VideoCodecProfile profile_;
  VideoPixelFormat pixel_format_;

  MockMediaLog media_log_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<VideoEncoder> encoder_;
  std::unique_ptr<VideoDecoder> decoder_;
};

class H264VideoEncoderTest : public SoftwareVideoEncoderTest {};
class SVCVideoEncoderTest : public SoftwareVideoEncoderTest {};

TEST_P(SoftwareVideoEncoderTest, InitializeAndFlush) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  bool output_called = false;
  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput, absl::optional<VideoEncoder::CodecDescription>) {
        output_called = true;
      });

  encoder_->Initialize(profile_, options, std::move(output_cb),
                       ValidatingStatusCB());
  RunUntilIdle();
  encoder_->Flush(ValidatingStatusCB());
  RunUntilIdle();
  EXPECT_FALSE(output_called) << "Output callback shouldn't be called";
}

TEST_P(SoftwareVideoEncoderTest, ForceAllKeyFrames) {
  int outputs_count = 0;
  int frames = 10;
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  auto frame_duration = base::Seconds(1.0 / 60);

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          absl::optional<VideoEncoder::CodecDescription> desc) {
        EXPECT_TRUE(output.key_frame);
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, std::move(output_cb),
                       ValidatingStatusCB());
  RunUntilIdle();

  for (int i = 0; i < frames; i++) {
    auto timestamp = i * frame_duration;
    auto frame = CreateFrame(options.frame_size, pixel_format_, timestamp);
    encoder_->Encode(frame, true, ValidatingStatusCB());
  }

  encoder_->Flush(ValidatingStatusCB());
  RunUntilIdle();
  EXPECT_EQ(outputs_count, frames);
}

TEST_P(SoftwareVideoEncoderTest, ResizeFrames) {
  int outputs_count = 0;
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  auto sec = base::Seconds(1);

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          absl::optional<VideoEncoder::CodecDescription> desc) {
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, std::move(output_cb),
                       ValidatingStatusCB());
  RunUntilIdle();

  auto frame1 = CreateFrame(gfx::Size(320, 200), pixel_format_, 0 * sec);
  auto frame2 = CreateFrame(gfx::Size(800, 600), pixel_format_, 1 * sec);
  auto frame3 = CreateFrame(gfx::Size(720, 1280), pixel_format_, 2 * sec);
  encoder_->Encode(frame1, false, ValidatingStatusCB());
  encoder_->Encode(frame2, false, ValidatingStatusCB());
  encoder_->Encode(frame3, false, ValidatingStatusCB());

  encoder_->Flush(ValidatingStatusCB());
  RunUntilIdle();
  EXPECT_EQ(outputs_count, 3);
}

TEST_P(SoftwareVideoEncoderTest, OutputCountEqualsFrameCount) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(320, 200);
  options.bitrate = Bitrate::VariableBitrate(1e6, 2e6);
  options.framerate = 25;
  options.keyframe_interval = options.framerate.value() * 3;  // every 3s
  int total_frames_count =
      options.framerate.value() * 10;  // total duration 20s
  int outputs_count = 0;

  auto frame_duration = base::Seconds(1.0 / options.framerate.value());

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          absl::optional<VideoEncoder::CodecDescription> desc) {
        EXPECT_NE(output.data, nullptr);
        EXPECT_EQ(output.timestamp, frame_duration * outputs_count);
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, std::move(output_cb),
                       ValidatingStatusCB());

  RunUntilIdle();
  uint32_t color = 0x964050;
  for (int frame_index = 0; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * frame_duration;
    auto frame =
        CreateFrame(options.frame_size, pixel_format_, timestamp, color);
    color = (color << 1) + frame_index;
    encoder_->Encode(frame, false, ValidatingStatusCB());
    RunUntilIdle();
  }

  encoder_->Flush(ValidatingStatusCB());
  RunUntilIdle();
  EXPECT_EQ(outputs_count, total_frames_count);
}

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
TEST_P(SoftwareVideoEncoderTest, EncodeAndDecode) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(320, 200);
  options.bitrate = Bitrate::ConstantBitrate(1e6);  // 1Mbps
  options.framerate = 25;
  if (codec_ == VideoCodec::kH264)
    options.avc.produce_annexb = true;
  options.keyframe_interval = options.framerate.value() * 3;  // every 3s
  std::vector<scoped_refptr<VideoFrame>> frames_to_encode;
  std::vector<scoped_refptr<VideoFrame>> decoded_frames;
  int total_frames_count =
      options.framerate.value() * 10;  // total duration 10s

  auto frame_duration = base::Seconds(1.0 / options.framerate.value());

  VideoEncoder::OutputCB encoder_output_cb = base::BindLambdaForTesting(
      [&, this](VideoEncoderOutput output,
                absl::optional<VideoEncoder::CodecDescription> desc) {
        auto buffer =
            DecoderBuffer::FromArray(std::move(output.data), output.size);
        buffer->set_timestamp(output.timestamp);
        buffer->set_is_key_frame(output.key_frame);
        decoder_->Decode(std::move(buffer), ValidatingStatusCB());
      });

  VideoDecoder::OutputCB decoder_output_cb =
      base::BindLambdaForTesting([&](scoped_refptr<VideoFrame> frame) {
        decoded_frames.push_back(frame);
      });

  PrepareDecoder(options.frame_size, std::move(decoder_output_cb));

  encoder_->Initialize(profile_, options, std::move(encoder_output_cb),
                       ValidatingStatusCB());
  RunUntilIdle();

  uint32_t color = 0x964050;
  for (int frame_index = 0; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * frame_duration;
    auto frame =
        CreateFrame(options.frame_size, pixel_format_, timestamp, color);
    frames_to_encode.push_back(frame);
    color = (color << 1) + frame_index;
    encoder_->Encode(frame, false, ValidatingStatusCB());
    RunUntilIdle();
  }

  encoder_->Flush(ValidatingStatusCB());
  DecodeAndWaitForStatus(DecoderBuffer::CreateEOSBuffer());
  EXPECT_EQ(decoded_frames.size(), frames_to_encode.size());
  for (auto i = 0u; i < decoded_frames.size(); i++) {
    auto original_frame = frames_to_encode[i];
    auto decoded_frame = decoded_frames[i];
    EXPECT_EQ(decoded_frame->timestamp(), original_frame->timestamp());
    EXPECT_EQ(decoded_frame->visible_rect(), original_frame->visible_rect());
    EXPECT_EQ(decoded_frame->format(), PIXEL_FORMAT_I420);
    if (decoded_frame->format() == original_frame->format()) {
      EXPECT_LE(CountDifferentPixels(*decoded_frame, *original_frame),
                original_frame->visible_rect().width());
    }
  }
}

TEST_P(SVCVideoEncoderTest, EncodeClipTemporalSvc) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(320, 200);
  options.bitrate = Bitrate::ConstantBitrate(1e6);  // 1Mbps
  options.framerate = 25;
  options.scalability_mode = GetParam().scalability_mode;
  if (codec_ == VideoCodec::kH264)
    options.avc.produce_annexb = true;
  std::vector<scoped_refptr<VideoFrame>> frames_to_encode;

  std::vector<VideoEncoderOutput> chunks;
  size_t total_frames_count = 80;

  // Encoder all frames with 3 temporal layers and put all outputs in |chunks|
  auto frame_duration = base::Seconds(1.0 / options.framerate.value());

  VideoEncoder::OutputCB encoder_output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          absl::optional<VideoEncoder::CodecDescription> desc) {
        chunks.push_back(std::move(output));
      });

  encoder_->Initialize(profile_, options, std::move(encoder_output_cb),
                       ValidatingStatusCB());
  RunUntilIdle();

  uint32_t color = 0x964050;
  for (auto frame_index = 0u; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * frame_duration;
    auto frame =
        CreateFrame(options.frame_size, pixel_format_, timestamp, color);
    color = (color << 1) + frame_index;
    frames_to_encode.push_back(frame);
    encoder_->Encode(frame, false, ValidatingStatusCB());
    RunUntilIdle();
  }

  encoder_->Flush(ValidatingStatusCB());
  RunUntilIdle();
  EXPECT_EQ(chunks.size(), total_frames_count);

  int num_temporal_layers = 1;
  if (options.scalability_mode) {
    switch (options.scalability_mode.value()) {
      case SVCScalabilityMode::kL1T2:
        num_temporal_layers = 2;
        break;
      case SVCScalabilityMode::kL1T3:
        num_temporal_layers = 3;
        break;
      default:
        NOTREACHED() << "Unsupported SVC: "
                     << GetScalabilityModeName(
                            options.scalability_mode.value());
    }
  }
  // Try decoding saved outputs dropping varying number of layers
  // and check that decoded frames indeed match the pattern:
  // Layer Index 0: |0| | | |4| | | |8| |  |  |12|
  // Layer Index 1: | | |2| | | |6| | | |10|  |  |
  // Layer Index 2: | |1| |3| |5| |7| |9|  |11|  |
  for (int max_layer = 0; max_layer < num_temporal_layers; max_layer++) {
    std::vector<scoped_refptr<VideoFrame>> decoded_frames;
    VideoDecoder::OutputCB decoder_output_cb =
        base::BindLambdaForTesting([&](scoped_refptr<VideoFrame> frame) {
          decoded_frames.push_back(frame);
        });
    PrepareDecoder(options.frame_size, std::move(decoder_output_cb));

    for (auto& chunk : chunks) {
      if (chunk.temporal_id <= max_layer && chunk.data) {
        auto buffer = DecoderBuffer::CopyFrom(chunk.data.get(), chunk.size);
        buffer->set_timestamp(chunk.timestamp);
        buffer->set_is_key_frame(chunk.key_frame);
        DecodeAndWaitForStatus(std::move(buffer));
      }
    }
    DecodeAndWaitForStatus(DecoderBuffer::CreateEOSBuffer());

    int rate_decimator = (1 << (num_temporal_layers - 1)) / (1 << max_layer);
    ASSERT_EQ(decoded_frames.size(),
              size_t{total_frames_count / rate_decimator});
    for (auto i = 0u; i < decoded_frames.size(); i++) {
      auto decoded_frame = decoded_frames[i];
      auto original_frame = frames_to_encode[i * rate_decimator];
      EXPECT_EQ(decoded_frame->timestamp(), original_frame->timestamp());
    }
  }
}
#endif  // ENABLE_FFMPEG_VIDEO_DECODERS

TEST_P(H264VideoEncoderTest, AvcExtraData) {
  int outputs_count = 0;
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  auto sec = base::Seconds(1);

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          absl::optional<VideoEncoder::CodecDescription> desc) {
        switch (outputs_count) {
          case 0:
            // First frame should have extra_data
            EXPECT_TRUE(desc.has_value());
            break;
          case 1:
            // Regular non-key frame shouldn't have extra_data
            EXPECT_FALSE(desc.has_value());
            break;
          case 2:
            // Forced Key frame should have extra_data
            EXPECT_TRUE(desc.has_value());
            break;
        }

        EXPECT_NE(output.data, nullptr);
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, std::move(output_cb),
                       ValidatingStatusCB());
  RunUntilIdle();

  auto frame1 = CreateFrame(options.frame_size, pixel_format_, 0 * sec);
  auto frame2 = CreateFrame(options.frame_size, pixel_format_, 1 * sec);
  auto frame3 = CreateFrame(options.frame_size, pixel_format_, 2 * sec);
  encoder_->Encode(frame1, false, ValidatingStatusCB());
  encoder_->Encode(frame2, false, ValidatingStatusCB());
  encoder_->Encode(frame3, true, ValidatingStatusCB());

  encoder_->Flush(ValidatingStatusCB());
  RunUntilIdle();
  EXPECT_EQ(outputs_count, 3);
}

TEST_P(H264VideoEncoderTest, AnnexB) {
  int outputs_count = 0;
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  options.avc.produce_annexb = true;
  auto sec = base::Seconds(1);

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          absl::optional<VideoEncoder::CodecDescription> desc) {
        EXPECT_FALSE(desc.has_value());
        EXPECT_NE(output.data, nullptr);

        // Check for a start code, it's either {0, 0, 1} or {0, 0, 0, 1}
        EXPECT_EQ(output.data[0], 0);
        EXPECT_EQ(output.data[1], 0);
        if (output.data[2] == 0)
          EXPECT_EQ(output.data[3], 1);
        else
          EXPECT_EQ(output.data[2], 1);
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, std::move(output_cb),
                       ValidatingStatusCB());
  RunUntilIdle();

  auto frame1 = CreateFrame(options.frame_size, pixel_format_, 0 * sec);
  auto frame2 = CreateFrame(options.frame_size, pixel_format_, 1 * sec);
  auto frame3 = CreateFrame(options.frame_size, pixel_format_, 2 * sec);
  encoder_->Encode(frame1, false, ValidatingStatusCB());
  encoder_->Encode(frame2, false, ValidatingStatusCB());
  encoder_->Encode(frame3, true, ValidatingStatusCB());

  encoder_->Flush(ValidatingStatusCB());
  RunUntilIdle();
  EXPECT_EQ(outputs_count, 3);
}

// This test is different from EncodeAndDecode:
// 1. It sets produce_annexb = false
// 2. It recreates the decoder each time there is new AVC extra data (SPS/PPS)
//    available.
TEST_P(H264VideoEncoderTest, EncodeAndDecodeWithConfig) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(320, 200);
  options.bitrate = Bitrate::ConstantBitrate(1e6);  // 1Mbps
  options.framerate = 25;
  options.avc.produce_annexb = false;
  struct ChunkWithConfig {
    VideoEncoderOutput output;
    absl::optional<VideoEncoder::CodecDescription> desc;
  };
  std::vector<scoped_refptr<VideoFrame>> frames_to_encode;
  std::vector<scoped_refptr<VideoFrame>> decoded_frames;
  std::vector<ChunkWithConfig> chunks;
  size_t total_frames_count = 30;
  auto frame_duration = base::Seconds(1.0 / options.framerate.value());

  VideoEncoder::OutputCB encoder_output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          absl::optional<VideoEncoder::CodecDescription> desc) {
        chunks.push_back({std::move(output), std::move(desc)});
      });

  encoder_->Initialize(profile_, options, std::move(encoder_output_cb),
                       ValidatingStatusCB());
  RunUntilIdle();

  uint32_t color = 0x964050;
  for (auto frame_index = 0u; frame_index < total_frames_count; frame_index++) {
    const auto timestamp = frame_index * frame_duration;
    const bool key_frame = (frame_index % 5) == 0;
    auto frame =
        CreateFrame(options.frame_size, pixel_format_, timestamp, color);
    frames_to_encode.push_back(frame);
    encoder_->Encode(frame, key_frame, ValidatingStatusCB());
    RunUntilIdle();
  }
  encoder_->Flush(ValidatingStatusCB());
  RunUntilIdle();

  EXPECT_EQ(chunks.size(), total_frames_count);
  for (auto& chunk : chunks) {
    VideoDecoder::OutputCB decoder_output_cb =
        base::BindLambdaForTesting([&](scoped_refptr<VideoFrame> frame) {
          decoded_frames.push_back(frame);
        });

    if (chunk.desc.has_value()) {
      if (decoder_)
        DecodeAndWaitForStatus(DecoderBuffer::CreateEOSBuffer());
      PrepareDecoder(options.frame_size, std::move(decoder_output_cb),
                     chunk.desc.value());
    }
    auto& output = chunk.output;
    auto buffer = DecoderBuffer::FromArray(std::move(output.data), output.size);
    buffer->set_timestamp(output.timestamp);
    buffer->set_is_key_frame(output.key_frame);
    DecodeAndWaitForStatus(std::move(buffer));
  }
  DecodeAndWaitForStatus(DecoderBuffer::CreateEOSBuffer());
  EXPECT_EQ(decoded_frames.size(), total_frames_count);
}

std::string PrintTestParams(
    const testing::TestParamInfo<SwVideoTestParams>& info) {
  auto result =
      GetCodecName(info.param.codec) + "__" +
      GetProfileName(info.param.profile) + "__" +
      VideoPixelFormatToString(info.param.pixel_format) + "__" +
      (info.param.scalability_mode
           ? GetScalabilityModeName(info.param.scalability_mode.value())
           : "");

  // GTest doesn't like spaces, but profile names have spaces, so we need
  // to replace them with underscores.
  for (auto& c : result) {
    if (c == ' ')
      c = '_';
  }
  return result;
}

#if BUILDFLAG(ENABLE_OPENH264)
SwVideoTestParams kH264Params[] = {
    {VideoCodec::kH264, H264PROFILE_BASELINE, PIXEL_FORMAT_I420},
    {VideoCodec::kH264, H264PROFILE_BASELINE, PIXEL_FORMAT_XRGB}};

INSTANTIATE_TEST_SUITE_P(H264Specific,
                         H264VideoEncoderTest,
                         ::testing::ValuesIn(kH264Params),
                         PrintTestParams);

INSTANTIATE_TEST_SUITE_P(H264Generic,
                         SoftwareVideoEncoderTest,
                         ::testing::ValuesIn(kH264Params),
                         PrintTestParams);

SwVideoTestParams kH264SVCParams[] = {
    {VideoCodec::kH264, H264PROFILE_BASELINE, PIXEL_FORMAT_I420, absl::nullopt},
    {VideoCodec::kH264, H264PROFILE_BASELINE, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T2},
    {VideoCodec::kH264, H264PROFILE_BASELINE, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T3}};

INSTANTIATE_TEST_SUITE_P(H264TemporalSvc,
                         SVCVideoEncoderTest,
                         ::testing::ValuesIn(kH264SVCParams),
                         PrintTestParams);
#endif  // ENABLE_OPENH264

#if BUILDFLAG(ENABLE_LIBVPX)
SwVideoTestParams kVpxParams[] = {
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_I420},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_XRGB},
    {VideoCodec::kVP8, VP8PROFILE_ANY, PIXEL_FORMAT_I420},
    {VideoCodec::kVP8, VP8PROFILE_ANY, PIXEL_FORMAT_XRGB}};

INSTANTIATE_TEST_SUITE_P(VpxGeneric,
                         SoftwareVideoEncoderTest,
                         ::testing::ValuesIn(kVpxParams),
                         PrintTestParams);

SwVideoTestParams kVpxSVCParams[] = {
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_I420, absl::nullopt},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T2},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T3},
    {VideoCodec::kVP8, VP8PROFILE_ANY, PIXEL_FORMAT_I420, absl::nullopt},
    {VideoCodec::kVP8, VP8PROFILE_ANY, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T2},
    {VideoCodec::kVP8, VP8PROFILE_ANY, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T3}};

INSTANTIATE_TEST_SUITE_P(VpxTemporalSvc,
                         SVCVideoEncoderTest,
                         ::testing::ValuesIn(kVpxSVCParams),
                         PrintTestParams);
#endif  // ENABLE_LIBVPX

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(H264VideoEncoderTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SVCVideoEncoderTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SoftwareVideoEncoderTest);

}  // namespace media
