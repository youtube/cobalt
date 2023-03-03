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

#include "starboard/nplb/player_test_util.h"

#include <functional>

#include "starboard/audio_sink.h"
#include "starboard/common/atomic.h"
#include "starboard/common/string.h"
#include "starboard/directory.h"
#include "starboard/extension/enhanced_audio.h"
#include "starboard/nplb/player_creation_param_helpers.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

namespace {

using shared::starboard::media::AudioSampleInfo;
using shared::starboard::media::VideoSampleInfo;
using shared::starboard::player::video_dmp::VideoDmpReader;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;
using testing::FakeGraphicsContextProvider;

void ErrorFunc(SbPlayer player,
               void* context,
               SbPlayerError error,
               const char* message) {
  atomic_bool* error_occurred = static_cast<atomic_bool*>(context);
  error_occurred->exchange(true);
}

}  // namespace

std::vector<SbPlayerTestConfig> GetSupportedSbPlayerTestConfigs() {
  const char* kAudioTestFiles[] = {"beneath_the_canopy_aac_stereo.dmp",
                                   "beneath_the_canopy_aac_5_1.dmp",
                                   "beneath_the_canopy_aac_mono.dmp",
                                   "beneath_the_canopy_opus_5_1.dmp",
                                   "beneath_the_canopy_opus_stereo.dmp",
                                   "beneath_the_canopy_opus_mono.dmp",
                                   "sintel_329_ec3.dmp",
                                   "sintel_381_ac3.dmp",
                                   "heaac.dmp"};

  const char* kVideoTestFiles[] = {"beneath_the_canopy_137_avc.dmp",
                                   "beneath_the_canopy_248_vp9.dmp",
                                   "sintel_399_av1.dmp"};

  const SbPlayerOutputMode kOutputModes[] = {kSbPlayerOutputModeDecodeToTexture,
                                             kSbPlayerOutputModePunchOut};

  std::vector<SbPlayerTestConfig> test_configs;

  const char* kEmptyName = NULL;

  for (auto audio_filename : kAudioTestFiles) {
    VideoDmpReader dmp_reader(ResolveTestFileName(audio_filename).c_str());
    SB_DCHECK(dmp_reader.number_of_audio_buffers() > 0);

    if (SbMediaCanPlayMimeAndKeySystem(dmp_reader.audio_mime_type().c_str(),
                                       "")) {
      for (auto output_mode : kOutputModes) {
        if (IsOutputModeSupported(output_mode, dmp_reader.audio_codec(),
                                  kSbMediaVideoCodecNone)) {
          test_configs.push_back(
              std::make_tuple(audio_filename, kEmptyName, output_mode));
        }
      }
    }
  }

  for (auto video_filename : kVideoTestFiles) {
    VideoDmpReader dmp_reader(ResolveTestFileName(video_filename).c_str());
    SB_DCHECK(dmp_reader.number_of_video_buffers() > 0);
    if (!SbMediaCanPlayMimeAndKeySystem(dmp_reader.video_mime_type().c_str(),
                                        "")) {
      continue;
    }
    for (auto output_mode : kOutputModes) {
      if (IsOutputModeSupported(output_mode, kSbMediaAudioCodecNone,
                                dmp_reader.video_codec())) {
        test_configs.push_back(
            std::make_tuple(kEmptyName, video_filename, output_mode));
      }
    }
  }

  return test_configs;
}

std::string ResolveTestFileName(const char* filename) {
  std::vector<char> content_path(kSbFileMaxPath);
  SB_CHECK(SbSystemGetPath(kSbSystemPathContentDirectory, content_path.data(),
                           kSbFileMaxPath));
  std::string directory_path = std::string(content_path.data()) +
                               kSbFileSepChar + "test" + kSbFileSepChar +
                               "starboard" + kSbFileSepChar + "shared" +
                               kSbFileSepChar + "starboard" + kSbFileSepChar +
                               "player" + kSbFileSepChar + "testdata";

  SB_CHECK(SbDirectoryCanOpen(directory_path.c_str()))
      << "Cannot open directory " << directory_path;
  return directory_path + kSbFileSepChar + filename;
}

void DummyDeallocateSampleFunc(SbPlayer player,
                               void* context,
                               const void* sample_buffer) {}

void DummyDecoderStatusFunc(SbPlayer player,
                            void* context,
                            SbMediaType type,
                            SbPlayerDecoderState state,
                            int ticket) {}

void DummyPlayerStatusFunc(SbPlayer player,
                           void* context,
                           SbPlayerState state,
                           int ticket) {}

void DummyErrorFunc(SbPlayer player,
                    void* context,
                    SbPlayerError error,
                    const char* message) {}

SbPlayer CallSbPlayerCreate(
    SbWindow window,
    SbMediaVideoCodec video_codec,
    SbMediaAudioCodec audio_codec,
    SbDrmSystem drm_system,
    const shared::starboard::media::AudioStreamInfo* audio_stream_info,
    const char* max_video_capabilities,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    void* context,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider* context_provider) {
  if (audio_stream_info) {
    SB_CHECK(audio_stream_info->codec == audio_codec);
  } else {
    SB_CHECK(audio_codec == kSbMediaAudioCodecNone);
  }

  PlayerCreationParam creation_param =
      CreatePlayerCreationParam(audio_codec, video_codec);
  if (audio_stream_info) {
    creation_param.audio_stream_info = *audio_stream_info;
  }
  creation_param.drm_system = drm_system;
  creation_param.output_mode = output_mode;
  creation_param.video_stream_info.max_video_capabilities =
      max_video_capabilities;

  SbPlayerCreationParam param = {};
  creation_param.ConvertTo(&param);
  return SbPlayerCreate(window, &param, sample_deallocate_func,
                        decoder_status_func, player_status_func,
                        player_error_func, context, context_provider);
}

void CallSbPlayerWriteSamples(
    SbPlayer player,
    SbMediaType sample_type,
    shared::starboard::player::video_dmp::VideoDmpReader* dmp_reader,
    int start_index,
    int number_of_samples_to_write) {
  SB_DCHECK(start_index >= 0);
  SB_DCHECK(number_of_samples_to_write > 0);

  static auto const* enhanced_audio_extension =
      static_cast<const CobaltExtensionEnhancedAudioApi*>(
          SbSystemGetExtension(kCobaltExtensionEnhancedAudioName));
#if SB_API_VERSION >= SB_MEDIA_ENHANCED_AUDIO_API_VERSION
  ASSERT_FALSE(enhanced_audio_extension);
#endif  // SB_API_VERSION >= SB_MEDIA_ENHANCED_AUDIO_API_VERSION

  if (enhanced_audio_extension) {
    ASSERT_STREQ(enhanced_audio_extension->name,
                 kCobaltExtensionEnhancedAudioName);
    ASSERT_EQ(enhanced_audio_extension->version, 1u);

    std::vector<CobaltExtensionEnhancedAudioPlayerSampleInfo> sample_infos;
    // We have to hold all intermediate sample infos to ensure that their member
    // variables with allocated memory (like `std::string mime`) won't go out of
    // scope before the call to `enhanced_audio_extension->PlayerWriteSamples`.
    std::vector<AudioSampleInfo> audio_sample_infos;
    std::vector<VideoSampleInfo> video_sample_infos;

    for (int i = 0; i < number_of_samples_to_write; ++i) {
      SbPlayerSampleInfo source =
          dmp_reader->GetPlayerSampleInfo(sample_type, start_index++);
      sample_infos.resize(sample_infos.size() + 1);
      sample_infos.back().type = source.type;
      sample_infos.back().buffer = source.buffer;
      sample_infos.back().buffer_size = source.buffer_size;
      sample_infos.back().timestamp = source.timestamp;
      sample_infos.back().side_data = source.side_data;
      sample_infos.back().side_data_count = source.side_data_count;
      sample_infos.back().drm_info = source.drm_info;

      if (sample_type == kSbMediaTypeAudio) {
        audio_sample_infos.emplace_back(source.audio_sample_info);
        audio_sample_infos.back().ConvertTo(
            &sample_infos.back().audio_sample_info);
      } else {
        SB_DCHECK(sample_type == kSbMediaTypeVideo);
        video_sample_infos.emplace_back(source.video_sample_info);
        video_sample_infos.back().ConvertTo(
            &sample_infos.back().video_sample_info);
      }
    }

    enhanced_audio_extension->PlayerWriteSamples(
        player, sample_type, sample_infos.data(), number_of_samples_to_write);

    return;
  }

  std::vector<SbPlayerSampleInfo> sample_infos;
  for (int i = 0; i < number_of_samples_to_write; ++i) {
    sample_infos.push_back(
        dmp_reader->GetPlayerSampleInfo(sample_type, start_index++));
  }
#if SB_API_VERSION >= SB_MEDIA_ENHANCED_AUDIO_API_VERSION
  SbPlayerWriteSamples(player, sample_type, sample_infos.data(),
                       number_of_samples_to_write);
#else   // SB_API_VERSION >= SB_MEDIA_ENHANCED_AUDIO_API_VERSION
  SbPlayerWriteSample2(player, sample_type, sample_infos.data(),
                       number_of_samples_to_write);
#endif  // SB_API_VERSION >= SB_MEDIA_ENHANCED_AUDIO_API_VERSION
}

bool IsOutputModeSupported(SbPlayerOutputMode output_mode,
                           SbMediaAudioCodec audio_codec,
                           SbMediaVideoCodec video_codec) {
  PlayerCreationParam creation_param =
      CreatePlayerCreationParam(audio_codec, video_codec);
  creation_param.output_mode = output_mode;

  SbPlayerCreationParam param = {};
  creation_param.ConvertTo(&param);
  return SbPlayerGetPreferredOutputMode(&param) == output_mode;
}

}  // namespace nplb
}  // namespace starboard
