// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/media_util.h"
#include "media/base/test_data_util.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/test/video.h"
#include "media/gpu/test/video_encoder/bitstream_file_writer.h"
#include "media/gpu/test/video_encoder/bitstream_validator.h"
#include "media/gpu/test/video_encoder/decoder_buffer_validator.h"
#include "media/gpu/test/video_encoder/video_encoder.h"
#include "media/gpu/test/video_encoder/video_encoder_client.h"
#include "media/gpu/test/video_encoder/video_encoder_test_environment.h"
#include "media/gpu/test/video_frame_file_writer.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "media/gpu/test/video_frame_validator.h"
#include "media/gpu/test/video_test_environment.h"
#include "media/gpu/test/video_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {
namespace test {

namespace {

// Video encoder tests usage message. Make sure to also update the documentation
// under docs/media/gpu/video_encoder_test_usage.md when making changes here.
// TODO(dstaessens): Add video_encoder_test_usage.md
constexpr const char* usage_msg =
    "usage: video_encode_accelerator_tests\n"
    "           [--codec=<codec>] [--num_temporal_layers=<number>]\n"
    "           [--num_spatial_layers=<number>] [--reverse]\n"
    "           [--disable_validator] [--output_bitstream]\n"
    "           [--output_images=(all|corrupt)] [--output_format=(png|yuv)]\n"
    "           [--output_folder=<filepath>] [--output_limit=<number>]\n"
    "           [-v=<level>] [--vmodule=<config>] [--gtest_help] [--help]\n"
    "           [<video path>] [<video metadata path>]\n";

// Video encoder tests help message.
constexpr const char* help_msg =
    "Run the video encoder accelerator tests on the video specified by\n"
    "<video path>. If no <video path> is given the default\n"
    "\"bear_320x192_40frames.yuv.webm\" video will be used.\n"
    "\nThe <video metadata path> should specify the location of a json file\n"
    "containing the video's metadata, such as frame checksums. By default\n"
    "<video path>.json will be used.\n"
    "\nThe following arguments are supported:\n"
    "  --codec               codec profile to encode, \"h264\" (baseline),\n"
    "                        \"h264main, \"h264high\", \"vp8\" and \"vp9\".\n"
    "                        H264 Baseline is selected if unspecified.\n"
    "  --num_temporal_layers the number of temporal layers of the encoded\n"
    "                        bitstream. Only used in --codec=vp9 and\n"
    "                        h264(baseline)|h264main|h264high currently.\n"
    "  --num_spatial_layers  the number of spatial layers of the encoded\n"
    "                        bitstream. Only used in --codec=vp9 currently.\n"
    "                        Spatial SVC encoding is applied only in\n"
    "                        NV12Dmabuf test cases.\n"
    "  --reverse             the stream plays backwards if the stream reaches\n"
    "                        end of stream. So the input stream to be encoded\n"
    "                        is consecutive. By default this is false. \n"
    "  --disable_validator   disable validation of encoded bitstream.\n"
    "  --output_bitstream    save the output bitstream in either H264 AnnexB\n"
    "                        format (for H264) or IVF format (for vp8 and\n"
    "                        vp9) to <output_folder>/<testname>.\n"
    "  --output_images       in addition to saving the full encoded,\n"
    "                        bitstream it's also possible to dump individual\n"
    "                        frames to <output_folder>/<testname>, possible\n"
    "                        values are \"all|corrupt\"\n"
    "  --output_format       set the format of images saved to disk,\n"
    "                        supported formats are \"png\" (default) and\n"
    "                        \"yuv\".\n"
    "  --output_limit        limit the number of images saved to disk.\n"
    "  --output_folder       set the basic folder used to store test\n"
    "                        artifacts. The default is the current directory.\n"
    "   -v                   enable verbose mode, e.g. -v=2.\n"
    "  --vmodule             enable verbose mode for the specified module,\n"
    "                        e.g. --vmodule=*media/gpu*=2.\n\n"
    "  --gtest_help          display the gtest help and exit.\n"
    "  --help                display this help and exit.\n";

// Default video to be used if no test video was specified.
constexpr base::FilePath::CharType kDefaultTestVideoPath[] =
    FILE_PATH_LITERAL("bear_320x192_40frames.yuv.webm");

// The number of frames to encode for bitrate check test cases.
// TODO(hiroh): Decrease this values to make the test faster.
constexpr size_t kNumFramesToEncodeForBitrateCheck = 300;
// Tolerance factor for how encoded bitrate can differ from requested bitrate.
constexpr double kBitrateTolerance = 0.1;
// The event timeout used in bitrate check tests because encoding 2160p and
// validating |kNumFramesToEncodeBitrateCheck| frames take much time.
constexpr base::TimeDelta kBitrateCheckEventTimeout = base::Seconds(180);

media::test::VideoEncoderTestEnvironment* g_env;

// Video encode test class. Performs setup and teardown for each single test.
class VideoEncoderTest : public ::testing::Test {
 public:
  // GetDefaultConfig() creates VideoEncoderClientConfig for SharedMemory input
  // encoding. This function must not be called in spatial SVC encoding.
  VideoEncoderClientConfig GetDefaultConfig() {
    const auto& spatial_layers = g_env->SpatialLayers();
    CHECK_LE(spatial_layers.size(), 1u);

    return VideoEncoderClientConfig(g_env->Video(), g_env->Profile(),
                                    spatial_layers, g_env->Bitrate(),
                                    g_env->Reverse());
  }

  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      Video* video,
      const VideoEncoderClientConfig& config) {
    LOG_ASSERT(video);

    auto video_encoder =
        VideoEncoder::Create(config, g_env->GetGpuMemoryBufferFactory(),
                             CreateBitstreamProcessors(video, config));
    LOG_ASSERT(video_encoder);

    if (!video_encoder->Initialize(video))
      ADD_FAILURE();

    return video_encoder;
  }

 private:
  std::unique_ptr<BitstreamProcessor> CreateBitstreamValidator(
      const Video* video,
      const VideoDecoderConfig& decoder_config,
      const size_t last_frame_index,
      VideoFrameValidator::GetModelFrameCB get_model_frame_cb,
      absl::optional<size_t> spatial_layer_index_to_decode,
      absl::optional<size_t> temporal_layer_index_to_decode,
      const std::vector<gfx::Size>& spatial_layer_resolutions) {
    std::vector<std::unique_ptr<VideoFrameProcessor>> video_frame_processors;

    // Attach a video frame writer to store individual frames to disk if
    // requested.
    std::unique_ptr<VideoFrameProcessor> image_writer;
    auto frame_output_config = g_env->ImageOutputConfig();
    base::FilePath output_folder = base::FilePath(g_env->OutputFolder())
                                       .Append(g_env->GetTestOutputFilePath());
    if (frame_output_config.output_mode != FrameOutputMode::kNone) {
      base::FilePath::StringType output_file_prefix;
      if (spatial_layer_index_to_decode) {
        output_file_prefix +=
            FILE_PATH_LITERAL("SL") +
            base::NumberToString(*spatial_layer_index_to_decode);
      }
      if (temporal_layer_index_to_decode) {
        output_file_prefix +=
            FILE_PATH_LITERAL("TL") +
            base::NumberToString(*temporal_layer_index_to_decode);
      }

      image_writer = VideoFrameFileWriter::Create(
          output_folder, frame_output_config.output_format,
          frame_output_config.output_limit, output_file_prefix);
      LOG_ASSERT(image_writer);
      if (frame_output_config.output_mode == FrameOutputMode::kAll)
        video_frame_processors.push_back(std::move(image_writer));
    }

    // For a resolution less than 360p, we lower the tolerance. Some platforms
    // couldn't compress a low resolution video efficiently with a low bitrate.
    constexpr gfx::Size k360p(640, 360);
    constexpr double kSSIMToleranceForLowerResolution = 0.65;
    const gfx::Size encode_resolution = decoder_config.visible_rect().size();
    const double ssim_tolerance =
        encode_resolution.GetArea() < k360p.GetArea()
            ? kSSIMToleranceForLowerResolution
            : SSIMVideoFrameValidator::kDefaultTolerance;

    auto ssim_validator = SSIMVideoFrameValidator::Create(
        get_model_frame_cb, std::move(image_writer),
        VideoFrameValidator::ValidationMode::kAverage, ssim_tolerance);
    LOG_ASSERT(ssim_validator);
    video_frame_processors.push_back(std::move(ssim_validator));
    return BitstreamValidator::Create(
        decoder_config, last_frame_index, std::move(video_frame_processors),
        spatial_layer_index_to_decode, temporal_layer_index_to_decode,
        spatial_layer_resolutions);
  }

  std::vector<std::unique_ptr<BitstreamProcessor>> CreateBitstreamProcessors(
      Video* video,
      const VideoEncoderClientConfig& config) {
    std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors;
    const gfx::Rect visible_rect(config.output_resolution);
    std::vector<gfx::Size> spatial_layer_resolutions;
    // |config.spatial_layers| is filled only in temporal layer or spatial layer
    // encoding.
    for (const auto& sl : config.spatial_layers)
      spatial_layer_resolutions.emplace_back(sl.width, sl.height);

    const VideoCodec codec =
        VideoCodecProfileToVideoCodec(config.output_profile);
    if (g_env->SaveOutputBitstream()) {
      base::FilePath::StringPieceType extension =
          codec == VideoCodec::kH264 ? FILE_PATH_LITERAL("h264")
                                     : FILE_PATH_LITERAL("ivf");
      auto output_bitstream_filepath =
          g_env->OutputFolder()
              .Append(g_env->GetTestOutputFilePath())
              .Append(video->FilePath().BaseName().ReplaceExtension(extension));
      if (!spatial_layer_resolutions.empty()) {
        CHECK_GE(config.num_spatial_layers, 1u);
        CHECK_GE(config.num_temporal_layers, 1u);
        for (size_t spatial_layer_index_to_write = 0;
             spatial_layer_index_to_write < config.num_spatial_layers;
             ++spatial_layer_index_to_write) {
          const gfx::Size& layer_size =
              spatial_layer_resolutions[spatial_layer_index_to_write];
          for (size_t temporal_layer_index_to_write = 0;
               temporal_layer_index_to_write < config.num_temporal_layers;
               ++temporal_layer_index_to_write) {
            bitstream_processors.emplace_back(BitstreamFileWriter::Create(
                output_bitstream_filepath.InsertBeforeExtensionASCII(
                    FILE_PATH_LITERAL(".SL") +
                    base::NumberToString(spatial_layer_index_to_write) +
                    FILE_PATH_LITERAL(".TL") +
                    base::NumberToString(temporal_layer_index_to_write)),
                codec, layer_size, config.framerate,
                config.num_frames_to_encode, spatial_layer_index_to_write,
                temporal_layer_index_to_write, spatial_layer_resolutions));
            LOG_ASSERT(bitstream_processors.back());
          }
        }
      } else {
        bitstream_processors.emplace_back(BitstreamFileWriter::Create(
            output_bitstream_filepath, codec, visible_rect.size(),
            config.framerate, config.num_frames_to_encode));
        LOG_ASSERT(bitstream_processors.back());
      }
    }

    if (!g_env->IsBitstreamValidatorEnabled()) {
      return bitstream_processors;
    }

    switch (codec) {
      case VideoCodec::kH264:
        bitstream_processors.emplace_back(new H264Validator(
            config.output_profile, visible_rect, config.num_temporal_layers));
        break;
      case VideoCodec::kVP8:
        bitstream_processors.emplace_back(new VP8Validator(visible_rect));
        break;
      case VideoCodec::kVP9:
        bitstream_processors.emplace_back(new VP9Validator(
            config.output_profile, visible_rect, config.num_spatial_layers,
            config.num_temporal_layers));
        break;
      default:
        LOG(ERROR) << "Unsupported profile: "
                   << GetProfileName(config.output_profile);
        break;
    }

    raw_data_helper_ = RawDataHelper::Create(video, g_env->Reverse());
    if (!raw_data_helper_) {
      LOG(ERROR) << "Failed to create raw data helper";
      return bitstream_processors;
    }

    if (!spatial_layer_resolutions.empty()) {
      CHECK_GE(config.num_spatial_layers, 1u);
      CHECK_GE(config.num_temporal_layers, 1u);
      for (size_t spatial_layer_index_to_decode = 0;
           spatial_layer_index_to_decode < config.num_spatial_layers;
           ++spatial_layer_index_to_decode) {
        const gfx::Size& layer_size =
            spatial_layer_resolutions[spatial_layer_index_to_decode];
        VideoDecoderConfig decoder_config(
            codec, config.output_profile,
            VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
            kNoTransformation, layer_size, gfx::Rect(layer_size), layer_size,
            EmptyExtraData(), EncryptionScheme::kUnencrypted);
        VideoFrameValidator::GetModelFrameCB get_model_frame_cb =
            base::BindRepeating(&VideoEncoderTest::GetModelFrame,
                                base::Unretained(this), gfx::Rect(layer_size));
        for (size_t temporal_layer_index_to_decode = 0;
             temporal_layer_index_to_decode < config.num_temporal_layers;
             ++temporal_layer_index_to_decode) {
          bitstream_processors.emplace_back(CreateBitstreamValidator(
              video, decoder_config, config.num_frames_to_encode - 1,
              get_model_frame_cb, spatial_layer_index_to_decode,
              temporal_layer_index_to_decode, spatial_layer_resolutions));
          LOG_ASSERT(bitstream_processors.back());
        }
      }
    } else {
      // Attach a bitstream validator to validate all encoded video frames. The
      // bitstream validator uses a software video decoder to validate the
      // encoded buffers by decoding them. Metrics such as the image's SSIM can
      // be calculated for additional quality checks.
      VideoDecoderConfig decoder_config(
          codec, config.output_profile,
          VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
          kNoTransformation, visible_rect.size(), visible_rect,
          visible_rect.size(), EmptyExtraData(),
          EncryptionScheme::kUnencrypted);
      VideoFrameValidator::GetModelFrameCB get_model_frame_cb =
          base::BindRepeating(&VideoEncoderTest::GetModelFrame,
                              base::Unretained(this), visible_rect);
      bitstream_processors.emplace_back(CreateBitstreamValidator(
          video, decoder_config, config.num_frames_to_encode - 1,
          get_model_frame_cb, absl::nullopt, absl::nullopt,
          /*spatial_layer_resolutions=*/{}));
      LOG_ASSERT(bitstream_processors.back());
    }
    return bitstream_processors;
  }

  scoped_refptr<const VideoFrame> GetModelFrame(const gfx::Rect& visible_rect,
                                                size_t frame_index) {
    LOG_ASSERT(raw_data_helper_);
    auto frame = raw_data_helper_->GetFrame(frame_index);
    if (!frame)
      return nullptr;
    if (visible_rect.size() == frame->visible_rect().size())
      return frame;
    return ScaleVideoFrame(frame.get(), visible_rect.size());
  }

  std::unique_ptr<RawDataHelper> raw_data_helper_;
};

absl::optional<std::string> SupportsDynamicFramerate() {
  return g_env->IsKeplerUsed()
             ? absl::make_optional<std::string>(
                   "The rate controller in the kepler firmware doesn't handle "
                   "frame rate changes correctly.")
             : absl::nullopt;
}

absl::optional<std::string> SupportsNV12DmaBufInput() {
  return g_env->IsKeplerUsed() ? absl::make_optional<std::string>(
                                     "Encoding with dmabuf input frames is not "
                                     "supported in kepler.")
                               : absl::nullopt;
}

}  // namespace

// Encode video from start to end. Wait for the kFlushDone event at the end of
// the stream, that notifies us all frames have been encoded.
TEST_F(VideoEncoderTest, FlushAtEndOfStream) {
  if (g_env->SpatialLayers().size() > 1)
    GTEST_SKIP() << "Skip SHMEM input test cases in spatial SVC encoding";

  auto encoder = CreateVideoEncoder(g_env->Video(), GetDefaultConfig());

  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

// Test initializing the video encoder. The test will be successful if the video
// encoder is capable of setting up the encoder for the specified codec and
// resolution. The test only verifies initialization and doesn't do any
// encoding.
TEST_F(VideoEncoderTest, Initialize) {
  if (g_env->SpatialLayers().size() > 1)
    GTEST_SKIP() << "Skip SHMEM input test cases in spatial SVC encoding";

  auto encoder = CreateVideoEncoder(g_env->Video(), GetDefaultConfig());

  EXPECT_EQ(encoder->GetEventCount(VideoEncoder::kInitialized), 1u);
}

// Create a video encoder and immediately destroy it without initializing. The
// video encoder will be automatically destroyed when the video encoder goes out
// of scope at the end of the test. The test will pass if no asserts or crashes
// are triggered upon destroying.
TEST_F(VideoEncoderTest, DestroyBeforeInitialize) {
  if (g_env->SpatialLayers().size() > 1)
    GTEST_SKIP() << "Skip SHMEM input test cases in spatial SVC encoding";

  auto video_encoder = VideoEncoder::Create(GetDefaultConfig(),
                                            g_env->GetGpuMemoryBufferFactory());

  EXPECT_NE(video_encoder, nullptr);
}

// Test forcing key frames while encoding a video.
TEST_F(VideoEncoderTest, ForceKeyFrame) {
  if (g_env->SpatialLayers().size() > 1)
    GTEST_SKIP() << "Skip SHMEM input test cases in spatial SVC encoding";

  auto config = GetDefaultConfig();
  const size_t middle_frame = config.num_frames_to_encode;
  config.num_frames_to_encode *= 2;
  auto encoder = CreateVideoEncoder(g_env->Video(), config);

  // It is expected that our hw encoders don't produce key frames in a short
  // time span like a few hundred frames.
  // TODO(hiroh): This might be wrong on some platforms. Needs to update.
  // Encode the first frame, this should always be a keyframe.
  encoder->EncodeUntil(VideoEncoder::kBitstreamReady, 1u);
  EXPECT_TRUE(encoder->WaitUntilIdle());
  EXPECT_EQ(encoder->GetEventCount(VideoEncoder::kKeyFrame), 1u);
  // Encode until the middle of stream and request force_keyframe.
  encoder->EncodeUntil(VideoEncoder::kFrameReleased, middle_frame);
  EXPECT_TRUE(encoder->WaitUntilIdle());
  // Check if there is no keyframe except the first frame.
  EXPECT_EQ(encoder->GetEventCount(VideoEncoder::kKeyFrame), 1u);
  encoder->ForceKeyFrame();
  // Since kFrameReleased and kBitstreamReady events are asynchronous, the
  // number of bitstreams being processed is unknown. We check keyframe request
  // is applied by seeing if there is a keyframe in a few frames after
  // requested. 10 is arbitrarily chosen.
  constexpr size_t kKeyFrameRequestWindow = 10u;
  encoder->EncodeUntil(VideoEncoder::kBitstreamReady,
                       std::min(middle_frame + kKeyFrameRequestWindow,
                                config.num_frames_to_encode));
  EXPECT_TRUE(encoder->WaitUntilIdle());
  EXPECT_EQ(encoder->GetEventCount(VideoEncoder::kKeyFrame), 2u);

  // Encode until the end of stream.
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_EQ(encoder->GetEventCount(VideoEncoder::kKeyFrame), 2u);
  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), config.num_frames_to_encode);
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

// Encode video from start to end. Multiple buffer encodes will be queued in the
// encoder, without waiting for the result of the previous encode requests.
TEST_F(VideoEncoderTest, FlushAtEndOfStream_MultipleOutstandingEncodes) {
  if (g_env->SpatialLayers().size() > 1)
    GTEST_SKIP() << "Skip SHMEM input test cases in spatial SVC encoding";

  auto config = GetDefaultConfig();
  config.max_outstanding_encode_requests = 4;
  auto encoder = CreateVideoEncoder(g_env->Video(), config);

  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

// Encode multiple videos simultaneously from start to finish.
TEST_F(VideoEncoderTest, FlushAtEndOfStream_MultipleConcurrentEncodes) {
  if (g_env->SpatialLayers().size() > 1)
    GTEST_SKIP() << "Skip SHMEM input test cases in spatial SVC encoding";

  // Run two encoders for larger resolutions to avoid creating shared memory
  // buffers during the test on lower end devices.
  constexpr gfx::Size k1080p(1920, 1080);
  const size_t kMinSupportedConcurrentEncoders =
      g_env->Video()->Resolution().GetArea() >= k1080p.GetArea() ? 2 : 3;

  auto config = GetDefaultConfig();
  std::vector<std::unique_ptr<VideoEncoder>> encoders(
      kMinSupportedConcurrentEncoders);
  for (size_t i = 0; i < kMinSupportedConcurrentEncoders; ++i)
    encoders[i] = CreateVideoEncoder(g_env->Video(), config);

  for (size_t i = 0; i < kMinSupportedConcurrentEncoders; ++i)
    encoders[i]->Encode();

  for (size_t i = 0; i < kMinSupportedConcurrentEncoders; ++i) {
    EXPECT_TRUE(encoders[i]->WaitForFlushDone());
    EXPECT_EQ(encoders[i]->GetFlushDoneCount(), 1u);
    EXPECT_EQ(encoders[i]->GetFrameReleasedCount(),
              g_env->Video()->NumFrames());
    EXPECT_TRUE(encoders[i]->WaitForBitstreamProcessors());
  }
}

TEST_F(VideoEncoderTest, BitrateCheck) {
  if (g_env->SpatialLayers().size() > 1)
    GTEST_SKIP() << "Skip SHMEM input test cases in spatial SVC encoding";

  auto config = GetDefaultConfig();
  config.num_frames_to_encode = kNumFramesToEncodeForBitrateCheck;
  auto encoder = CreateVideoEncoder(g_env->Video(), config);
  // Set longer event timeout than the default (30 sec) because encoding 2160p
  // and validating the stream take much time.
  encoder->SetEventWaitTimeout(kBitrateCheckEventTimeout);

  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), config.num_frames_to_encode);
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
  EXPECT_NEAR(encoder->GetStats().Bitrate(), config.bitrate.GetSumBps(),
              kBitrateTolerance * config.bitrate.GetSumBps());
}

TEST_F(VideoEncoderTest, BitrateCheck_DynamicBitrate) {
  if (g_env->SpatialLayers().size() > 1)
    GTEST_SKIP() << "Skip SHMEM input test cases in spatial SVC encoding";

  auto config = GetDefaultConfig();
  config.num_frames_to_encode = kNumFramesToEncodeForBitrateCheck * 2;
  auto encoder = CreateVideoEncoder(g_env->Video(), config);
  // Set longer event timeout than the default (30 sec) because encoding 2160p
  // and validating the stream take much time.
  encoder->SetEventWaitTimeout(kBitrateCheckEventTimeout);

  // Encode the video with the first bitrate.
  const uint32_t first_bitrate = config.bitrate.GetSumBps();
  encoder->EncodeUntil(VideoEncoder::kFrameReleased,
                       kNumFramesToEncodeForBitrateCheck);
  EXPECT_TRUE(encoder->WaitUntilIdle());
  EXPECT_NEAR(encoder->GetStats().Bitrate(), first_bitrate,
              kBitrateTolerance * first_bitrate);

  // Encode the video with the second bitrate.
  const uint32_t second_bitrate = first_bitrate * 3 / 2;
  encoder->ResetStats();
  encoder->UpdateBitrate(
      g_env->GetDefaultVideoBitrateAllocation(second_bitrate),
      config.framerate);
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_NEAR(encoder->GetStats().Bitrate(), second_bitrate,
              kBitrateTolerance * second_bitrate);

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), config.num_frames_to_encode);
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

TEST_F(VideoEncoderTest, BitrateCheck_DynamicFramerate) {
  if (g_env->SpatialLayers().size() > 1)
    GTEST_SKIP() << "Skip SHMEM input test cases in spatial SVC encoding";

  if (auto skip_reason = SupportsDynamicFramerate())
    GTEST_SKIP() << *skip_reason;
  auto config = GetDefaultConfig();
  config.num_frames_to_encode = kNumFramesToEncodeForBitrateCheck * 2;
  auto encoder = CreateVideoEncoder(g_env->Video(), config);
  // Set longer event timeout than the default (30 sec) because encoding 2160p
  // and validating the stream take much time.
  encoder->SetEventWaitTimeout(kBitrateCheckEventTimeout);

  // Encode the video with the first framerate.
  const uint32_t first_framerate = config.framerate;

  encoder->EncodeUntil(VideoEncoder::kFrameReleased,
                       kNumFramesToEncodeForBitrateCheck);
  EXPECT_TRUE(encoder->WaitUntilIdle());
  EXPECT_NEAR(encoder->GetStats().Bitrate(), config.bitrate.GetSumBps(),
              kBitrateTolerance * config.bitrate.GetSumBps());

  // Encode the video with the second framerate.
  const uint32_t second_framerate = first_framerate * 3 / 2;
  encoder->ResetStats();
  encoder->UpdateBitrate(config.bitrate, second_framerate);
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_NEAR(encoder->GetStats().Bitrate(), config.bitrate.GetSumBps(),
              kBitrateTolerance * config.bitrate.GetSumBps());

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), config.num_frames_to_encode);
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

TEST_F(VideoEncoderTest, FlushAtEndOfStream_NV12Dmabuf) {
  if (auto skip_reason = SupportsNV12DmaBufInput())
    GTEST_SKIP() << *skip_reason;

  Video* nv12_video = g_env->GenerateNV12Video();
  VideoEncoderClientConfig config(nv12_video, g_env->Profile(),
                                  g_env->SpatialLayers(), g_env->Bitrate(),
                                  g_env->Reverse());
  config.input_storage_type =
      VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;

  auto encoder = CreateVideoEncoder(nv12_video, config);

  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), nv12_video->NumFrames());
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

// Downscaling is required in VideoEncodeAccelerator when zero-copy video
// capture is enabled. One example is simulcast, camera produces 360p VideoFrame
// and there are two VideoEncodeAccelerator for 360p and 180p. VideoEncoder for
// 180p is fed 360p and thus has to perform the scaling from 360p to 180p.
TEST_F(VideoEncoderTest, FlushAtEndOfStream_NV12DmabufScaling) {
  if (auto skip_reason = SupportsNV12DmaBufInput())
    GTEST_SKIP() << *skip_reason;
  if (g_env->SpatialLayers().size() > 1)
    GTEST_SKIP() << "Skip simulcast test case for spatial SVC encoding";

  constexpr gfx::Size kMinOutputResolution(240, 180);
  const gfx::Size output_resolution =
      gfx::Size(g_env->Video()->Resolution().width() / 2,
                g_env->Video()->Resolution().height() / 2);
  if (!gfx::Rect(output_resolution).Contains(gfx::Rect(kMinOutputResolution))) {
    GTEST_SKIP() << "Skip test if video resolution is too small, "
                 << "output_resolution=" << output_resolution.ToString()
                 << ", minimum output resolution="
                 << kMinOutputResolution.ToString();
  }

  auto* nv12_video = g_env->GenerateNV12Video();
  // Set 1/4 of the original bitrate because the area of |output_resolution| is
  // 1/4 of the original resolution.
  uint32_t new_bitrate = g_env->Bitrate().GetSumBps() / 4;
  auto spatial_layers = g_env->SpatialLayers();
  if (!spatial_layers.empty()) {
    CHECK_EQ(spatial_layers.size(), 1u);
    spatial_layers[0].width = output_resolution.width();
    spatial_layers[0].height = output_resolution.height();
    spatial_layers[0].bitrate_bps /= 4;
  }
  VideoEncoderClientConfig config(
      nv12_video, g_env->Profile(), spatial_layers,
      g_env->GetDefaultVideoBitrateAllocation(new_bitrate), g_env->Reverse());
  config.output_resolution = output_resolution;
  config.input_storage_type =
      VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;

  auto encoder = CreateVideoEncoder(nv12_video, config);
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), nv12_video->NumFrames());
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

// Encode VideoFrames with cropping the rectangle (0, 60, size).
// Cropping is required in VideoEncodeAccelerator when zero-copy video
// capture is enabled. One example is when 640x360 capture recording is
// requested, a camera cannot produce the resolution and instead produces
// 640x480 frames with visible_rect=0, 60, 640x360.
TEST_F(VideoEncoderTest, FlushAtEndOfStream_NV12DmabufCroppingTopAndBottom) {
  if (auto skip_reason = SupportsNV12DmaBufInput())
    GTEST_SKIP() << *skip_reason;
  constexpr int kGrowHeight = 120;
  const gfx::Size original_resolution = g_env->Video()->Resolution();
  const gfx::Rect expanded_visible_rect(0, kGrowHeight / 2,
                                        original_resolution.width(),
                                        original_resolution.height());
  const gfx::Size expanded_resolution(
      original_resolution.width(), original_resolution.height() + kGrowHeight);
  constexpr gfx::Size kMaxExpandedResolution(1920, 1080);
  if (!gfx::Rect(kMaxExpandedResolution)
           .Contains(gfx::Rect(expanded_resolution))) {
    GTEST_SKIP() << "Expanded video resolution is too large, "
                 << "expanded_resolution=" << expanded_resolution.ToString()
                 << ", maximum expanded resolution="
                 << kMaxExpandedResolution.ToString();
  }

  auto nv12_expanded_video = g_env->GenerateNV12Video()->Expand(
      expanded_resolution, expanded_visible_rect);
  ASSERT_TRUE(nv12_expanded_video);
  VideoEncoderClientConfig config(nv12_expanded_video.get(), g_env->Profile(),
                                  g_env->SpatialLayers(), g_env->Bitrate(),
                                  g_env->Reverse());
  config.output_resolution = original_resolution;
  config.input_storage_type =
      VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;

  auto encoder = CreateVideoEncoder(nv12_expanded_video.get(), config);
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), nv12_expanded_video->NumFrames());
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

// Encode VideoFrames with cropping the rectangle (60, 0, size).
// Cropping is required in VideoEncodeAccelerator when zero-copy video
// capture is enabled. One example is when 640x360 capture recording is
// requested, a camera cannot produce the resolution and instead produces
// 760x360 frames with visible_rect=60, 0, 640x360.
TEST_F(VideoEncoderTest, FlushAtEndOfStream_NV12DmabufCroppingRightAndLeft) {
  if (auto skip_reason = SupportsNV12DmaBufInput())
    GTEST_SKIP() << *skip_reason;
  constexpr int kGrowWidth = 120;
  const gfx::Size original_resolution = g_env->Video()->Resolution();
  const gfx::Rect expanded_visible_rect(kGrowWidth / 2, 0,
                                        original_resolution.width(),
                                        original_resolution.height());
  const gfx::Size expanded_resolution(original_resolution.width() + kGrowWidth,
                                      original_resolution.height());
  constexpr gfx::Size kMaxExpandedResolution(1920, 1080);
  if (!gfx::Rect(kMaxExpandedResolution)
           .Contains(gfx::Rect(expanded_resolution))) {
    GTEST_SKIP() << "Expanded video resolution is too large, "
                 << "expanded_resolution=" << expanded_resolution.ToString()
                 << ", maximum expanded resolution="
                 << kMaxExpandedResolution.ToString();
  }

  auto nv12_expanded_video = g_env->GenerateNV12Video()->Expand(
      expanded_resolution, expanded_visible_rect);
  ASSERT_TRUE(nv12_expanded_video);
  VideoEncoderClientConfig config(nv12_expanded_video.get(), g_env->Profile(),
                                  g_env->SpatialLayers(), g_env->Bitrate(),
                                  g_env->Reverse());
  config.output_resolution = original_resolution;
  config.input_storage_type =
      VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;

  auto encoder = CreateVideoEncoder(nv12_expanded_video.get(), config);
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), nv12_expanded_video->NumFrames());
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

// This tests deactivate and activating spatial layers during encoding.
TEST_F(VideoEncoderTest, DeactivateAndActivateSpatialLayers) {
  if (auto skip_reason = SupportsNV12DmaBufInput())
    GTEST_SKIP() << *skip_reason;

  const auto& spatial_layers = g_env->SpatialLayers();
  if (spatial_layers.size() <= 1)
    GTEST_SKIP() << "Skip (de)activate spatial layers test for simple encoding";

  Video* nv12_video = g_env->GenerateNV12Video();
  const size_t bottom_spatial_idx = 0;
  const size_t top_spatial_idx = spatial_layers.size() - 1;
  auto deactivate_spatial_layer =
      [](VideoBitrateAllocation bitrate_allocation,
         size_t deactivate_sid) -> VideoBitrateAllocation {
    for (size_t i = 0; i < VideoBitrateAllocation::kMaxTemporalLayers; ++i)
      bitrate_allocation.SetBitrate(deactivate_sid, i, 0);
    return bitrate_allocation;
  };

  const auto& default_allocation = g_env->Bitrate();
  std::vector<VideoBitrateAllocation> bitrate_allocations;

  // Deactivate the top layer.
  bitrate_allocations.emplace_back(
      deactivate_spatial_layer(default_allocation, top_spatial_idx));

  // Activate the top layer.
  bitrate_allocations.emplace_back(default_allocation);

  // Deactivate the bottom layer (and top layer if there is still a spatial
  // layer).
  auto bitrate_allocation =
      deactivate_spatial_layer(default_allocation, bottom_spatial_idx);
  if (bottom_spatial_idx + 1 < top_spatial_idx) {
    bitrate_allocation =
        deactivate_spatial_layer(bitrate_allocation, top_spatial_idx);
  }
  bitrate_allocations.emplace_back(bitrate_allocation);

  // Deactivate the layers except bottom layer.
  bitrate_allocation = default_allocation;
  for (size_t i = bottom_spatial_idx + 1; i < spatial_layers.size(); ++i)
    bitrate_allocation = deactivate_spatial_layer(bitrate_allocation, i);
  bitrate_allocations.emplace_back(bitrate_allocation);

  VideoEncoderClientConfig config(nv12_video, g_env->Profile(),
                                  g_env->SpatialLayers(), g_env->Bitrate(),
                                  g_env->Reverse());
  config.input_storage_type =
      VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;
  std::vector<size_t> num_frames_to_encode(bitrate_allocations.size());
  for (size_t i = 0; i < num_frames_to_encode.size(); ++i)
    num_frames_to_encode[i] = config.num_frames_to_encode * (i + 1);
  config.num_frames_to_encode =
      num_frames_to_encode.back() + config.num_frames_to_encode;

  auto encoder = CreateVideoEncoder(nv12_video, config);

  for (size_t i = 0; i < bitrate_allocations.size(); ++i) {
    encoder->EncodeUntil(VideoEncoder::kFrameReleased, num_frames_to_encode[i]);
    EXPECT_TRUE(encoder->WaitUntilIdle());
    encoder->UpdateBitrate(bitrate_allocations[i], config.framerate);
  }

  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), config.num_frames_to_encode);
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

}  // namespace test
}  // namespace media

int main(int argc, char** argv) {
  // Set the default test data path.
  media::test::Video::SetTestDataPath(media::GetTestDataPath());

  // Print the help message if requested. This needs to be done before
  // initializing gtest, to overwrite the default gtest help message.
  base::CommandLine::Init(argc, argv);
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  LOG_ASSERT(cmd_line);
  if (cmd_line->HasSwitch("help")) {
    std::cout << media::test::usage_msg << "\n" << media::test::help_msg;
    return 0;
  }

  // Check if a video was specified on the command line.
  base::CommandLine::StringVector args = cmd_line->GetArgs();
  base::FilePath video_path =
      (args.size() >= 1) ? base::FilePath(args[0])
                         : base::FilePath(media::test::kDefaultTestVideoPath);
  base::FilePath video_metadata_path =
      (args.size() >= 2) ? base::FilePath(args[1]) : base::FilePath();
  std::string codec = "h264";
  size_t num_temporal_layers = 1u;
  size_t num_spatial_layers = 1u;
  bool output_bitstream = false;
  bool reverse = false;
  media::test::FrameOutputConfig frame_output_config;
  base::FilePath output_folder =
      base::FilePath(base::FilePath::kCurrentDirectory);

  // Parse command line arguments.
  bool enable_bitstream_validator = true;
  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first.find("gtest_") == 0 ||               // Handled by GoogleTest
        it->first == "v" || it->first == "vmodule") {  // Handled by Chrome
      continue;
    }

    if (it->first == "codec") {
      codec = it->second;
    } else if (it->first == "num_temporal_layers") {
      if (!base::StringToSizeT(it->second, &num_temporal_layers)) {
        std::cout << "invalid number of temporal layers: " << it->second
                  << "\n";
        return EXIT_FAILURE;
      }
    } else if (it->first == "num_spatial_layers") {
      if (!base::StringToSizeT(it->second, &num_spatial_layers)) {
        std::cout << "invalid number of spatial layers: " << it->second << "\n";
        return EXIT_FAILURE;
      }
    } else if (it->first == "disable_validator") {
      enable_bitstream_validator = false;
    } else if (it->first == "output_bitstream") {
      output_bitstream = true;
    } else if (it->first == "reverse") {
      reverse = true;
    } else if (it->first == "output_images") {
      if (it->second == "all") {
        frame_output_config.output_mode = media::test::FrameOutputMode::kAll;
      } else if (it->second == "corrupt") {
        frame_output_config.output_mode =
            media::test::FrameOutputMode::kCorrupt;
      } else {
        std::cout << "unknown image output mode \"" << it->second
                  << "\", possible values are \"all|corrupt\"\n";
        return EXIT_FAILURE;
      }
    } else if (it->first == "output_format") {
      if (it->second == "png") {
        frame_output_config.output_format =
            media::test::VideoFrameFileWriter::OutputFormat::kPNG;
      } else if (it->second == "yuv") {
        frame_output_config.output_format =
            media::test::VideoFrameFileWriter::OutputFormat::kYUV;
      } else {
        std::cout << "unknown frame output format \"" << it->second
                  << "\", possible values are \"png|yuv\"\n";
        return EXIT_FAILURE;
      }
    } else if (it->first == "output_limit") {
      if (!base::StringToUint64(it->second,
                                &frame_output_config.output_limit)) {
        std::cout << "invalid number \"" << it->second << "\n";
        return EXIT_FAILURE;
      }
    } else if (it->first == "output_folder") {
      output_folder = base::FilePath(it->second);
    } else {
      std::cout << "unknown option: --" << it->first << "\n"
                << media::test::usage_msg;
      return EXIT_FAILURE;
    }
  }

  testing::InitGoogleTest(&argc, argv);

  // Set up our test environment.
  media::test::VideoEncoderTestEnvironment* test_environment =
      media::test::VideoEncoderTestEnvironment::Create(
          video_path, video_metadata_path, enable_bitstream_validator,
          output_folder, codec, num_temporal_layers, num_spatial_layers,
          output_bitstream,
          /*output_bitrate=*/absl::nullopt, reverse, frame_output_config);

  if (!test_environment)
    return EXIT_FAILURE;

  media::test::g_env = static_cast<media::test::VideoEncoderTestEnvironment*>(
      testing::AddGlobalTestEnvironment(test_environment));

  return RUN_ALL_TESTS();
}
