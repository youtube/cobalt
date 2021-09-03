// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/testing/test_util.h"

#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/directory.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/filter/stub_player_components_factory.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/string.h"
#include "starboard/system.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace testing {
namespace {

using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;
using video_dmp::VideoDmpReader;

std::string GetTestInputDirectory() {
  const size_t kPathSize = kSbFileMaxPath + 1;

  std::vector<char> content_path(kPathSize);
  SB_CHECK(SbSystemGetPath(kSbSystemPathContentDirectory, content_path.data(),
                           kPathSize));
  std::string directory_path = std::string(content_path.data()) +
                               kSbFileSepChar + "test" + kSbFileSepChar +
                               "starboard" + kSbFileSepChar + "shared" +
                               kSbFileSepChar + "starboard" + kSbFileSepChar +
                               "player" + kSbFileSepChar + "testdata";

  SB_CHECK(SbDirectoryCanOpen(directory_path.c_str()))
      << "Cannot open directory " << directory_path;
  return directory_path;
}

}  // namespace

void StubDeallocateSampleFunc(SbPlayer player,
                              void* context,
                              const void* sample_buffer) {}

std::string ResolveTestFileName(const char* filename) {
  return GetTestInputDirectory() + kSbFileSepChar + filename;
}

std::string GetContentTypeFromAudioCodec(SbMediaAudioCodec audio_codec,
                                         const char* mime_attributes) {
  SB_DCHECK(audio_codec != kSbMediaAudioCodecNone);

  std::string content_type;
  switch (audio_codec) {
    case kSbMediaAudioCodecAac:
      content_type = "audio/mp4; codecs=\"mp4a.40.2\"";
      break;
    case kSbMediaAudioCodecOpus:
      content_type = "audio/webm; codecs=\"opus\"";
      break;
    case kSbMediaAudioCodecAc3:
      content_type = "audio/mp4; codecs=\"ac-3\"";
      break;
    case kSbMediaAudioCodecEac3:
      content_type = "audio/mp4; codecs=\"ec-3\"";
      break;
    default:
      SB_NOTREACHED();
  }
  return strlen(mime_attributes) > 0 ? content_type + "; " + mime_attributes
                                     : content_type;
}

std::vector<const char*> GetSupportedAudioTestFiles(
    HeaacOption heaac_option,
    int max_channels,
    const char* extra_mime_attributes) {
  // beneath_the_canopy_aac_stereo.dmp
  //   codec: kSbMediaAudioCodecAac
  //   sampling rate: 44.1k
  //   frames per AU: 1024
  // beneath_the_canopy_opus_stereo.dmp
  //   codec: kSbMediaAudioCodecOpus
  //   sampling rate: 48.0k
  //   frames per AU: 960

  // IMPORTANT: do not change the order of test files unless necessary. If you
  // must change the order of these tests please make sure you update the test
  // filters for every platform, just in case a particular test case should be
  // disabled.

  struct AudioFileInfo {
    const char* filename;
    SbMediaAudioCodec audio_codec;
    int num_channels;
    int64_t bitrate;
    bool is_heaac;
  };

  const char* kFilenames[] = {"beneath_the_canopy_aac_stereo.dmp",
                              "beneath_the_canopy_aac_5_1.dmp",
                              "beneath_the_canopy_aac_mono.dmp",
                              "beneath_the_canopy_opus_5_1.dmp",
                              "beneath_the_canopy_opus_stereo.dmp",
                              "beneath_the_canopy_opus_mono.dmp",
                              "sintel_329_ec3.dmp",
                              "sintel_381_ac3.dmp",
                              "heaac.dmp"};

  static std::vector<AudioFileInfo> audio_file_info_cache;
  std::vector<const char*> filenames;

  if (audio_file_info_cache.empty()) {
    audio_file_info_cache.reserve(SB_ARRAY_SIZE_INT(kFilenames));
    for (auto filename : kFilenames) {
      VideoDmpReader dmp_reader(ResolveTestFileName(filename).c_str(),
                                VideoDmpReader::kEnableReadOnDemand);
      SB_DCHECK(dmp_reader.number_of_audio_buffers() > 0);

      audio_file_info_cache.push_back(
          {filename, dmp_reader.audio_codec(),
           dmp_reader.audio_sample_info().number_of_channels,
           dmp_reader.audio_bitrate(), strstr(filename, "heaac") != nullptr});
    }
  }

  for (auto& audio_file_info : audio_file_info_cache) {
    // Filter files with channels exceeding |max_channels|.
    if (audio_file_info.num_channels > max_channels) {
      continue;
    }

    // Filter heaac files when |heaac_option| == kExcludeHeaac.
    if (heaac_option == kExcludeHeaac && audio_file_info.is_heaac) {
      continue;
    }

    // Filter files of unsupported codec.
    if (!SbMediaIsAudioSupported(
            audio_file_info.audio_codec,
#if SB_API_VERSION >= 12
            GetContentTypeFromAudioCodec(audio_file_info.audio_codec,
                                         extra_mime_attributes)
                .c_str(),
#endif  // SB_API_VERSION >= 12
            audio_file_info.bitrate)) {
      continue;
    }

    filenames.push_back(audio_file_info.filename);
  }

  return filenames;
}

std::vector<VideoTestParam> GetSupportedVideoTests() {
  SbPlayerOutputMode kOutputModes[] = {kSbPlayerOutputModeDecodeToTexture,
                                       kSbPlayerOutputModePunchOut};

  // IMPORTANT: do not change the order of test files unless necessary. If you
  // must change the order of these tests please make sure you update the test
  // filters for every platform, just in case a particular test case should be
  // disabled.

  const char* kFilenames[] = {
      "beneath_the_canopy_137_avc.dmp", "beneath_the_canopy_248_vp9.dmp",
      "sintel_399_av1.dmp", "black_test_avc_1080p_30to60_fps.dmp"};

  static std::vector<VideoTestParam> test_params;

  if (!test_params.empty()) {
    return test_params;
  }

  for (auto filename : kFilenames) {
    VideoDmpReader dmp_reader(ResolveTestFileName(filename).c_str(),
                              VideoDmpReader::kEnableReadOnDemand);
    SB_DCHECK(dmp_reader.number_of_video_buffers() > 0);

    for (auto output_mode : kOutputModes) {
      if (!VideoDecoder::OutputModeSupported(
              output_mode, dmp_reader.video_codec(), kSbDrmSystemInvalid)) {
        continue;
      }

      const auto& video_sample_info =
          dmp_reader.GetPlayerSampleInfo(kSbMediaTypeVideo, 0)
              .video_sample_info;

      if (SbMediaIsVideoSupported(
              dmp_reader.video_codec(),
#if SB_API_VERSION >= 12
              dmp_reader.video_mime_type().c_str(),
#endif  // SB_API_VERSION >= 12
#if SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
              -1, -1, 8, kSbMediaPrimaryIdUnspecified,
              kSbMediaTransferIdUnspecified, kSbMediaMatrixIdUnspecified,
#endif  // SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
              video_sample_info.frame_width, video_sample_info.frame_height,
              dmp_reader.video_bitrate(), dmp_reader.video_fps(), false)) {
        test_params.push_back(std::make_tuple(filename, output_mode));
      }
    }
  }

  SB_DCHECK(!test_params.empty());
  return test_params;
}

bool CreateAudioComponents(bool using_stub_decoder,
                           SbMediaAudioCodec codec,
                           const SbMediaAudioSampleInfo& audio_sample_info,
                           scoped_ptr<AudioDecoder>* audio_decoder,
                           scoped_ptr<AudioRendererSink>* audio_renderer_sink) {
  SB_CHECK(audio_decoder);
  SB_CHECK(audio_renderer_sink);

  audio_renderer_sink->reset();
  audio_decoder->reset();

  PlayerComponents::Factory::CreationParameters creation_parameters(
      codec, audio_sample_info);

  scoped_ptr<PlayerComponents::Factory> factory;
  if (using_stub_decoder) {
    factory = StubPlayerComponentsFactory::Create();
  } else {
    factory = PlayerComponents::Factory::Create();
  }
  std::string error_message;
  if (factory->CreateSubComponents(creation_parameters, audio_decoder,
                                   audio_renderer_sink, nullptr, nullptr,
                                   nullptr, &error_message)) {
    SB_CHECK(*audio_decoder);
    return true;
  }
  audio_renderer_sink->reset();
  audio_decoder->reset();
  return false;
}

AssertionResult AlmostEqualTime(SbTime time1, SbTime time2) {
  const SbTime kEpsilon = kSbTimeSecond / 1000;
  SbTime diff = time1 - time2;
  if (-kEpsilon <= diff && diff <= kEpsilon) {
    return AssertionSuccess();
  }
  return AssertionFailure()
         << "time " << time1 << " doesn't match with time " << time2;
}

#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
media::VideoSampleInfo CreateVideoSampleInfo(SbMediaVideoCodec codec) {
  shared::starboard::media::VideoSampleInfo video_sample_info = {};

  video_sample_info.codec = codec;
  video_sample_info.mime = "";
  video_sample_info.max_video_capabilities = "";

  video_sample_info.color_metadata.primaries = kSbMediaPrimaryIdBt709;
  video_sample_info.color_metadata.transfer = kSbMediaTransferIdBt709;
  video_sample_info.color_metadata.matrix = kSbMediaMatrixIdBt709;
  video_sample_info.color_metadata.range = kSbMediaRangeIdLimited;

  video_sample_info.frame_width = 1920;
  video_sample_info.frame_height = 1080;

  return video_sample_info;
}
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
