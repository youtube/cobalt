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

#include <unistd.h>

#include "starboard/audio_sink.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/media/mime_type.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/filter/stub_player_components_factory.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/system.h"

namespace starboard::shared::starboard::player::filter::testing {
namespace {

using ::starboard::shared::starboard::media::MimeType;
using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;
using video_dmp::VideoDmpReader;

}  // namespace

void StubDeallocateSampleFunc(SbPlayer player,
                              void* context,
                              const void* sample_buffer) {}

std::string GetContentTypeFromAudioCodec(SbMediaAudioCodec audio_codec,
                                         const char* mime_attributes) {
  SB_DCHECK_NE(audio_codec, kSbMediaAudioCodecNone);

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
    case kSbMediaAudioCodecIamf:
      content_type = "audio/mp4; codecs=\"iamf\"";
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
                              "heaac.dmp",
                              "iamf_base_profile_stereo_ambisonics.dmp",
                              "iamf_simple_profile_5_1.dmp",
                              "sintel_329_ec3.dmp",
                              "sintel_381_ac3.dmp"};

  static std::vector<AudioFileInfo> audio_file_info_cache;
  std::vector<const char*> filenames;

  if (audio_file_info_cache.empty()) {
    audio_file_info_cache.reserve(SB_ARRAY_SIZE_INT(kFilenames));
    for (auto filename : kFilenames) {
      VideoDmpReader dmp_reader(filename, VideoDmpReader::kEnableReadOnDemand);
      SB_DCHECK_GT(dmp_reader.number_of_audio_buffers(), 0);

      audio_file_info_cache.push_back(
          {filename, dmp_reader.audio_codec(),
           dmp_reader.audio_stream_info().number_of_channels,
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
    const std::string audio_mime = GetContentTypeFromAudioCodec(
        audio_file_info.audio_codec, extra_mime_attributes);
    const MimeType audio_mime_type(audio_mime.c_str());
    if (!MediaIsAudioSupported(audio_file_info.audio_codec, &audio_mime_type,
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
    VideoDmpReader dmp_reader(filename, VideoDmpReader::kEnableReadOnDemand);
    SB_DCHECK_GT(dmp_reader.number_of_video_buffers(), 0);

    for (auto output_mode : kOutputModes) {
      if (!PlayerComponents::Factory::OutputModeSupported(
              output_mode, dmp_reader.video_codec(), kSbDrmSystemInvalid)) {
        continue;
      }

      const auto& video_stream_info = dmp_reader.video_stream_info();
      const std::string video_mime = dmp_reader.video_mime_type();
      const MimeType video_mime_type(video_mime.c_str());
      // MediaIsVideoSupported may return false for gpu based decoder that in
      // fact supports av1 or/and vp9 because the system can make async
      // initialization at startup.
      // To minimize probability of false negative we check result few times
      static bool decoder_has_been_checked_once = false;
      int counter = 5;
      const SbMediaVideoCodec video_codec = dmp_reader.video_codec();
      bool need_to_check_with_wait = video_codec == kSbMediaVideoCodecAv1 ||
                                     video_codec == kSbMediaVideoCodecVp9;
      do {
        if (MediaIsVideoSupported(
                video_codec, video_mime.size() > 0 ? &video_mime_type : nullptr,
                -1, -1, 8, kSbMediaPrimaryIdUnspecified,
                kSbMediaTransferIdUnspecified, kSbMediaMatrixIdUnspecified,
                video_stream_info.frame_width, video_stream_info.frame_height,
                dmp_reader.video_bitrate(), dmp_reader.video_fps(), false)) {
          test_params.push_back(std::make_tuple(filename, output_mode));
          break;
        } else if (need_to_check_with_wait && !decoder_has_been_checked_once) {
          usleep(1'000'000);
        } else {
          break;
        }
      } while (--counter);
      if (need_to_check_with_wait) {
        decoder_has_been_checked_once = true;
      }
    }
  }

  SB_DCHECK(!test_params.empty());
  return test_params;
}

bool CreateAudioComponents(
    bool using_stub_decoder,
    const media::AudioStreamInfo& audio_stream_info,
    std::unique_ptr<AudioDecoder>* audio_decoder,
    std::unique_ptr<AudioRendererSink>* audio_renderer_sink) {
  SB_CHECK(audio_decoder);
  SB_CHECK(audio_renderer_sink);

  audio_renderer_sink->reset();
  audio_decoder->reset();

  PlayerComponents::Factory::CreationParameters creation_parameters(
      audio_stream_info);

  std::unique_ptr<PlayerComponents::Factory> factory;
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

AssertionResult AlmostEqualTime(int64_t time1, int64_t time2) {
  const int64_t kEpsilon = 1000;  // 1ms
  int64_t diff = time1 - time2;
  if (-kEpsilon <= diff && diff <= kEpsilon) {
    return AssertionSuccess();
  }
  return AssertionFailure()
         << "time " << time1 << " doesn't match with time " << time2;
}

media::VideoStreamInfo CreateVideoStreamInfo(SbMediaVideoCodec codec) {
  shared::starboard::media::VideoStreamInfo video_stream_info = {};

  video_stream_info.codec = codec;
  video_stream_info.mime = "";
  video_stream_info.max_video_capabilities = "";

  video_stream_info.color_metadata.bits_per_channel = 8;
  video_stream_info.color_metadata.primaries = kSbMediaPrimaryIdBt709;
  video_stream_info.color_metadata.transfer = kSbMediaTransferIdBt709;
  video_stream_info.color_metadata.matrix = kSbMediaMatrixIdBt709;
  video_stream_info.color_metadata.range = kSbMediaRangeIdLimited;

  video_stream_info.frame_width = 1920;
  video_stream_info.frame_height = 1080;

  return video_stream_info;
}

bool IsPartialAudioSupported() {
  return true;
}

scoped_refptr<InputBuffer> GetAudioInputBuffer(
    video_dmp::VideoDmpReader* dmp_reader,
    size_t index) {
  SB_DCHECK(dmp_reader);

  auto player_sample_info =
      dmp_reader->GetPlayerSampleInfo(kSbMediaTypeAudio, index);
  return new InputBuffer(StubDeallocateSampleFunc, nullptr, nullptr,
                         player_sample_info);
}

scoped_refptr<InputBuffer> GetAudioInputBuffer(
    video_dmp::VideoDmpReader* dmp_reader,
    size_t index,
    int64_t discarded_duration_from_front,
    int64_t discarded_duration_from_back) {
  SB_DCHECK(dmp_reader);
  auto player_sample_info =
      dmp_reader->GetPlayerSampleInfo(kSbMediaTypeAudio, index);
  player_sample_info.audio_sample_info.discarded_duration_from_front =
      discarded_duration_from_front;
  player_sample_info.audio_sample_info.discarded_duration_from_back =
      discarded_duration_from_back;
  auto input_buffer = new InputBuffer(StubDeallocateSampleFunc, nullptr,
                                      nullptr, player_sample_info);
  return input_buffer;
}

}  // namespace starboard::shared::starboard::player::filter::testing
