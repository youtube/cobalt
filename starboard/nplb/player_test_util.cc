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
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <utility>

#include "starboard/audio_sink.h"
#include "starboard/common/atomic.h"
#include "starboard/common/string.h"
#include "starboard/extension/enhanced_audio.h"
#include "starboard/nplb/drm_helpers.h"
#include "starboard/nplb/maximum_player_configuration_explorer.h"
#include "starboard/nplb/player_creation_param_helpers.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

namespace {

using shared::starboard::media::AudioSampleInfo;
using shared::starboard::media::AudioStreamInfo;
using shared::starboard::media::VideoSampleInfo;
using shared::starboard::player::video_dmp::VideoDmpReader;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;
using testing::FakeGraphicsContextProvider;

const char* kAudioTestFiles[] = {"beneath_the_canopy_aac_stereo.dmp",
                                 "beneath_the_canopy_opus_stereo.dmp",
                                 "sintel_329_ec3.dmp", "sintel_381_ac3.dmp"};

// For uncommon audio formats, we add audio only tests, without tests combined
// with a video stream, to shorten the overall test time.
const char* kAudioOnlyTestFiles[] = {
    "beneath_the_canopy_aac_5_1.dmp", "beneath_the_canopy_aac_mono.dmp",
    "beneath_the_canopy_opus_5_1.dmp", "beneath_the_canopy_opus_mono.dmp",
    "heaac.dmp"};

const char* kVideoTestFiles[] = {"beneath_the_canopy_137_avc.dmp",
                                 "beneath_the_canopy_248_vp9.dmp",
                                 "sintel_399_av1.dmp"};

const SbPlayerOutputMode kOutputModes[] = {kSbPlayerOutputModeDecodeToTexture,
                                           kSbPlayerOutputModePunchOut};

void ErrorFunc(SbPlayer player,
               void* context,
               SbPlayerError error,
               const char* message) {
  atomic_bool* error_occurred = static_cast<atomic_bool*>(context);
  error_occurred->exchange(true);
}

}  // namespace

std::vector<const char*> GetAudioTestFiles() {
  return std::vector<const char*>(std::begin(kAudioTestFiles),
                                  std::end(kAudioTestFiles));
}

std::vector<const char*> GetVideoTestFiles() {
  return std::vector<const char*>(std::begin(kVideoTestFiles),
                                  std::end(kVideoTestFiles));
}

std::vector<SbPlayerOutputMode> GetPlayerOutputModes() {
  return std::vector<SbPlayerOutputMode>(std::begin(kOutputModes),
                                         std::end(kOutputModes));
}

std::vector<SbPlayerTestConfig> GetSupportedSbPlayerTestConfigs(
    const char* key_system) {
  SB_DCHECK(key_system);

  const char* kEmptyName = NULL;

  std::vector<const char*> supported_audio_files;
  supported_audio_files.push_back(kEmptyName);
  for (auto audio_filename : kAudioTestFiles) {
    VideoDmpReader dmp_reader(audio_filename,
                              VideoDmpReader::kEnableReadOnDemand);
    SB_DCHECK(dmp_reader.number_of_audio_buffers() > 0);
    if (SbMediaCanPlayMimeAndKeySystem(dmp_reader.audio_mime_type().c_str(),
                                       key_system)) {
      supported_audio_files.push_back(audio_filename);
    }
  }

  std::vector<const char*> supported_video_files;
  supported_video_files.push_back(kEmptyName);
  for (auto video_filename : kVideoTestFiles) {
    VideoDmpReader dmp_reader(video_filename,
                              VideoDmpReader::kEnableReadOnDemand);
    SB_DCHECK(dmp_reader.number_of_video_buffers() > 0);
    if (SbMediaCanPlayMimeAndKeySystem(dmp_reader.video_mime_type().c_str(),
                                       key_system)) {
      supported_video_files.push_back(video_filename);
    }
  }

  std::vector<SbPlayerTestConfig> test_configs;
  for (auto audio_filename : supported_audio_files) {
    SbMediaAudioCodec audio_codec = kSbMediaAudioCodecNone;
    if (audio_filename) {
      VideoDmpReader audio_dmp_reader(audio_filename,
                                      VideoDmpReader::kEnableReadOnDemand);
      audio_codec = audio_dmp_reader.audio_codec();
    }
    for (auto video_filename : supported_video_files) {
      SbMediaVideoCodec video_codec = kSbMediaVideoCodecNone;
      if (video_filename) {
        VideoDmpReader video_dmp_reader(video_filename,
                                        VideoDmpReader::kEnableReadOnDemand);
        video_codec = video_dmp_reader.video_codec();
      }
      if (audio_codec == kSbMediaAudioCodecNone &&
          video_codec == kSbMediaVideoCodecNone) {
        continue;
      }

      for (auto output_mode : kOutputModes) {
        if (IsOutputModeSupported(output_mode, audio_codec, video_codec,
                                  key_system)) {
          test_configs.push_back(std::make_tuple(audio_filename, video_filename,
                                                 output_mode, key_system));
        }
      }
    }
  }

  for (auto audio_filename : kAudioOnlyTestFiles) {
    VideoDmpReader dmp_reader(audio_filename,
                              VideoDmpReader::kEnableReadOnDemand);
    SB_DCHECK(dmp_reader.number_of_audio_buffers() > 0);
    if (SbMediaCanPlayMimeAndKeySystem(dmp_reader.audio_mime_type().c_str(),
                                       key_system)) {
      for (auto output_mode : kOutputModes) {
        if (IsOutputModeSupported(output_mode, dmp_reader.audio_codec(),
                                  kSbMediaVideoCodecNone, key_system)) {
          test_configs.push_back(std::make_tuple(audio_filename, kEmptyName,
                                                 output_mode, key_system));
        }
      }
    }
  }

  return test_configs;
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
      CreatePlayerCreationParam(audio_codec, video_codec, output_mode);
  if (audio_stream_info) {
    creation_param.audio_stream_info = *audio_stream_info;
  }
  creation_param.drm_system = drm_system;
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
#if SB_API_VERSION >= 15
  ASSERT_FALSE(enhanced_audio_extension);
#endif  // SB_API_VERSION >= 15

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
#if SB_API_VERSION >= 15
  SbPlayerWriteSamples(player, sample_type, sample_infos.data(),
                       number_of_samples_to_write);
#else   // SB_API_VERSION >= 15
  SbPlayerWriteSample2(player, sample_type, sample_infos.data(),
                       number_of_samples_to_write);
#endif  // SB_API_VERSION >= 15
}

bool IsOutputModeSupported(SbPlayerOutputMode output_mode,
                           SbMediaAudioCodec audio_codec,
                           SbMediaVideoCodec video_codec,
                           const char* key_system) {
  SB_DCHECK(key_system);

  PlayerCreationParam creation_param =
      CreatePlayerCreationParam(audio_codec, video_codec, output_mode);

  SbPlayerCreationParam param = {};
  creation_param.ConvertTo(&param);

  if (strlen(key_system) > 0) {
    param.drm_system = SbDrmCreateSystem(
        key_system, NULL /* context */, DummySessionUpdateRequestFunc,
        DummySessionUpdatedFunc, DummySessionKeyStatusesChangedFunc,
        DummyServerCertificateUpdatedFunc, DummySessionClosedFunc);

    if (!SbDrmSystemIsValid(param.drm_system)) {
      return false;
    }
  }

  bool supported = SbPlayerGetPreferredOutputMode(&param) == output_mode;
  if (SbDrmSystemIsValid(param.drm_system)) {
    SbDrmDestroySystem(param.drm_system);
  }
  return supported;
}

std::vector<SbPlayerMultiplePlayerTestConfig> GetMultiplePlayerTestConfig(
    SbPlayerOutputMode output_mode,
    const char* key_system,
    int max_player_instances_per_config,
    int total_instances_limitation,
    FakeGraphicsContextProvider* fake_graphics_context_provider) {
  SB_DCHECK(fake_graphics_context_provider);

  std::vector<SbPlayerMultiplePlayerTestConfig> result;
  MaximumPlayerConfigurationExplorer::PlayerConfigSet video_test_configs;
  MaximumPlayerConfigurationExplorer::PlayerConfigSet audio_test_configs;

  std::map<SbMediaVideoCodec, std::string> video_codec_to_test_file;
  std::map<SbMediaAudioCodec, std::string> audio_codec_to_test_file;

  for (auto video_filename : kVideoTestFiles) {
    VideoDmpReader dmp_reader(video_filename,
                              VideoDmpReader::kEnableReadOnDemand);
    SB_DCHECK(dmp_reader.number_of_video_buffers() > 0);

    if (SbMediaCanPlayMimeAndKeySystem(dmp_reader.video_mime_type().c_str(),
                                       "")) {
      if (IsOutputModeSupported(output_mode, kSbMediaAudioCodecNone,
                                dmp_reader.video_codec())) {
        video_test_configs.insert(
            MaximumPlayerConfigurationExplorer::PlayerConfig(
                dmp_reader.video_codec(), AudioStreamInfo(), output_mode,
                key_system));
        video_codec_to_test_file[dmp_reader.video_codec()] = video_filename;
      }
    }
  }

  // We must retain the VideoDmpReader of the audio because the lifetime of the
  // AudioStreamInfo depends on it.
  std::vector<std::unique_ptr<VideoDmpReader>> audio_dmp_readers;
  for (auto audio_filename : kAudioTestFiles) {
    audio_dmp_readers.emplace_back(
        std::make_unique<VideoDmpReader>(audio_filename));
    const auto& dmp_reader = audio_dmp_readers.back();
    SB_DCHECK(dmp_reader->number_of_audio_buffers() > 0);

    if (SbMediaCanPlayMimeAndKeySystem(dmp_reader->audio_mime_type().c_str(),
                                       "")) {
      if (IsOutputModeSupported(output_mode, dmp_reader->audio_codec(),
                                kSbMediaVideoCodecNone)) {
        const shared::starboard::media::AudioStreamInfo audio_stream_info =
            dmp_reader->audio_stream_info();
        MaximumPlayerConfigurationExplorer::PlayerConfig player_config(
            kSbMediaVideoCodecNone, audio_stream_info, output_mode, key_system);
        audio_test_configs.insert(player_config);
        audio_codec_to_test_file[dmp_reader->audio_codec()] = audio_filename;
      }
    }
  }

  auto total_instance_limitation_function =
      [total_instances_limitation](const std::vector<int>& v) -> bool {
    int sum_of_elements = std::accumulate(v.begin(), v.end(), 0);
    return sum_of_elements <= total_instances_limitation;
  };

  // Determine the supported configurations for video-only players.
  // We have an upper limit for each video configuration.
  MaximumPlayerConfigurationExplorer video_explorer(
      video_test_configs, max_player_instances_per_config,
      fake_graphics_context_provider, total_instance_limitation_function);
  auto max_video_config_set = video_explorer.CalculateMaxTestConfigSet();

  // Determine the supported configurations for audio-only players.
  // There are no upper limits for each audio configuration.
  MaximumPlayerConfigurationExplorer audio_explorer(
      audio_test_configs, max_player_instances_per_config,
      fake_graphics_context_provider);
  auto max_audio_config_set = audio_explorer.CalculateMaxTestConfigSet();

  // We select the audio codec using a round-robin method. Initially, we choose
  // the maximum elements set with the highest number of supported audio only
  // players. For example, if the maximum elements set is {(1, 2, 3), (2, 1, 1),
  // {4, 0, 3}}, (4, 0, 3) will be selected since it supports 7 players in
  // total.
  int max_audio_instances = 0;
  std::vector<int> max_audio_config_with_max_instances;
  for (const auto& max_audio_config : max_audio_config_set) {
    int audio_instances = 0;
    for (auto num : max_audio_config) {
      audio_instances += num;
    }
    if (audio_instances > max_audio_instances) {
      max_audio_config_with_max_instances = max_audio_config;
      max_audio_instances = audio_instances;
    }
  }

  // Create an index array |audio_codec_candidates| to keep track of the indices
  // we will use. For instance, if the maximum elements of audio players are [2,
  // 1, 3], the index array would be [0, 1, 2, 0, 2, 2].
  std::vector<int> audio_codec_candidates(max_audio_instances, 0);
  int codec_index = max_audio_config_with_max_instances.size() - 1;
  for (int i = 0; i < max_audio_instances; ++i) {
    int j;
    for (j = 0; j < max_audio_config_with_max_instances.size(); ++j) {
      codec_index =
          codec_index == max_audio_config_with_max_instances.size() - 1
              ? 0
              : codec_index + 1;
      int num_of_instance = max_audio_config_with_max_instances[codec_index];
      if (max_audio_config_with_max_instances[codec_index] > 0) {
        --max_audio_config_with_max_instances[codec_index];
        break;
      }
    }
    SB_DCHECK(j != max_audio_config_with_max_instances.size());

    audio_codec_candidates[i] = codec_index;
  }

  // Generate test configurations for multiple players, ensuring that each
  // player configuration includes a video codec. The audio codec is selected
  // from the index array |audio_codec_candidates|. If no more audio codecs are
  // available, set it to kSbMediaAudioCodecNone (and set the audio DMP file
  // path to null).
  for (const auto& max_video_config : max_video_config_set) {
    SB_DCHECK(max_video_config.size() == video_test_configs.size());

    SbPlayerMultiplePlayerTestConfig multiple_player_test_config;
    int audio_codec_index = 0;
    for (int i = 0; i < max_video_config.size(); ++i) {
      const auto& video_test_config = *std::next(video_test_configs.begin(), i);
      auto video_dmp_file_path =
          video_codec_to_test_file[std::get<0>(video_test_config)].c_str();
      for (int j = 0; j < max_video_config[i]; ++j) {
        SbPlayerTestConfig player_test_config;
        const char* audio_dmp_file_path;
        if (audio_codec_index < audio_codec_candidates.size()) {
          const auto& audio_test_config =
              *std::next(audio_test_configs.begin(),
                         audio_codec_candidates[audio_codec_index++]);
          audio_dmp_file_path =
              audio_codec_to_test_file[std::get<1>(audio_test_config).codec]
                  .c_str();
        } else {
          audio_dmp_file_path = nullptr;
        }
        std::get<0>(player_test_config) = video_dmp_file_path;
        std::get<1>(player_test_config) = audio_dmp_file_path;
        std::get<2>(player_test_config) = output_mode;
        multiple_player_test_config.push_back(player_test_config);
      }
    }
    result.push_back(multiple_player_test_config);
  }

  return result;
}

}  // namespace nplb
}  // namespace starboard
