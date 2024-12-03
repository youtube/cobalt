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

#include <atomic>
#include <functional>

#include "starboard/audio_sink.h"
#include "starboard/common/string.h"
#include "starboard/extension/enhanced_audio.h"
#include "starboard/nplb/drm_helpers.h"
#include "starboard/nplb/maximum_player_configuration_explorer.h"
#include "starboard/nplb/player_creation_param_helpers.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
// TODO(cobalt, b/377295011): remove the nogncheck annotation.
#include "starboard/testing/fake_graphics_context_provider.h"  // nogncheck
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

const char* kAudioTestFiles[] = {
    "beneath_the_canopy_aac_stereo.dmp",
    "beneath_the_canopy_opus_stereo.dmp",
    "sintel_329_ec3.dmp",
    "sintel_381_ac3.dmp",
};

// For uncommon audio formats, we add audio only tests, without tests combined
// with a video stream, to shorten the overall test time.
const char* kAudioOnlyTestFiles[] = {
    "beneath_the_canopy_aac_5_1.dmp",
    "beneath_the_canopy_aac_mono.dmp",
    "beneath_the_canopy_opus_5_1.dmp",
    "beneath_the_canopy_opus_mono.dmp",
    "heaac.dmp",
    "iamf_base_profile_stereo_ambisonics.dmp",
    "iamf_simple_profile_5_1.dmp",
    "sintel_5s_pcm_s16le.dmp",
    "sintel_5s_flac.dmp",
#if defined(ENABLE_CAST_CODEC_TESTS)
    "sintel_5s_mp3.dmp",
    "sintel_5s_vorbis.dmp",
#endif
};

const char* kVideoTestFiles[] = {
    "beneath_the_canopy_137_avc.dmp",
    "beneath_the_canopy_248_vp9.dmp",
    "sintel_399_av1.dmp",
#if defined(ENABLE_CAST_CODEC_TESTS)
    "sintel_5s_vp8.dmp",
    "sintel_5s_hevc.dmp",
#endif
};

const SbPlayerOutputMode kOutputModes[] = {kSbPlayerOutputModeDecodeToTexture,
                                           kSbPlayerOutputModePunchOut};

void ErrorFunc(SbPlayer player,
               void* context,
               SbPlayerError error,
               const char* message) {
  std::atomic_bool* error_occurred = static_cast<std::atomic_bool*>(context);
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

std::vector<const char*> GetKeySystems() {
  std::vector<const char*> key_systems;
  key_systems.push_back("");
  key_systems.insert(key_systems.end(), kKeySystems,
                     kKeySystems + SB_ARRAY_SIZE_INT(kKeySystems));
  return key_systems;
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
          test_configs.emplace_back(audio_filename, video_filename, output_mode,
                                    key_system);
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
          test_configs.emplace_back(audio_filename, kEmptyName, output_mode,
                                    key_system);
        }
      }
    }
  }

  return test_configs;
}

std::string GetSbPlayerTestConfigName(
    ::testing::TestParamInfo<SbPlayerTestConfig> info) {
  const SbPlayerTestConfig& config = info.param;
  const char* audio_filename = config.audio_filename;
  const char* video_filename = config.video_filename;
  const SbPlayerOutputMode output_mode = config.output_mode;
  const char* key_system = config.key_system;
  std::string name(FormatString(
      "audio_%s_video_%s_output_%s_key_system_%s",
      audio_filename && strlen(audio_filename) > 0 ? audio_filename : "null",
      video_filename && strlen(video_filename) > 0 ? video_filename : "null",
      output_mode == kSbPlayerOutputModeDecodeToTexture ? "decode_to_texture"
                                                        : "punch_out",
      strlen(key_system) > 0 ? key_system : "null"));
  std::replace(name.begin(), name.end(), '.', '_');
  std::replace(name.begin(), name.end(), '(', '_');
  std::replace(name.begin(), name.end(), ')', '_');
  return name;
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

  // TODO: pass real audio/video info to SbPlayerGetPreferredOutputMode.
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
    int number_of_samples_to_write,
    int64_t timestamp_offset,
    const std::vector<int64_t>& discarded_durations_from_front,
    const std::vector<int64_t>& discarded_durations_from_back) {
  SB_DCHECK(start_index >= 0);
  SB_DCHECK(number_of_samples_to_write > 0);

  if (sample_type == kSbMediaTypeAudio) {
    SB_DCHECK(discarded_durations_from_front.empty() ||
              discarded_durations_from_front.size() ==
                  number_of_samples_to_write);
    SB_DCHECK(discarded_durations_from_front.size() ==
              discarded_durations_from_back.size());
  } else {
    SB_DCHECK(sample_type == kSbMediaTypeVideo);
    SB_DCHECK(discarded_durations_from_front.empty());
    SB_DCHECK(discarded_durations_from_back.empty());
  }

  static auto const* enhanced_audio_extension =
      static_cast<const CobaltExtensionEnhancedAudioApi*>(
          SbSystemGetExtension(kCobaltExtensionEnhancedAudioName));
  ASSERT_FALSE(enhanced_audio_extension);

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
      sample_infos.back().timestamp = source.timestamp + timestamp_offset;
      sample_infos.back().side_data = source.side_data;
      sample_infos.back().side_data_count = source.side_data_count;
      sample_infos.back().drm_info = source.drm_info;

      if (sample_type == kSbMediaTypeAudio) {
        audio_sample_infos.emplace_back(source.audio_sample_info);
        audio_sample_infos.back().ConvertTo(
            &sample_infos.back().audio_sample_info);
        if (!discarded_durations_from_front.empty()) {
          sample_infos.back().audio_sample_info.discarded_duration_from_front =
              discarded_durations_from_front[i];
        }
        if (!discarded_durations_from_back.empty()) {
          sample_infos.back().audio_sample_info.discarded_duration_from_back =
              discarded_durations_from_back[i];
        }
      } else {
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
    sample_infos.back().timestamp += timestamp_offset;
    if (!discarded_durations_from_front.empty()) {
      sample_infos.back().audio_sample_info.discarded_duration_from_front =
          discarded_durations_from_front[i];
    }
    if (!discarded_durations_from_back.empty()) {
      sample_infos.back().audio_sample_info.discarded_duration_from_back =
          discarded_durations_from_back[i];
    }
  }
  SbPlayerWriteSamples(player, sample_type, sample_infos.data(),
                       number_of_samples_to_write);
}

bool IsOutputModeSupported(SbPlayerOutputMode output_mode,
                           SbMediaAudioCodec audio_codec,
                           SbMediaVideoCodec video_codec,
                           const char* key_system) {
  SB_DCHECK(key_system);

  // TODO: pass real audio/video info to SbPlayerGetPreferredOutputMode.
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

bool IsPartialAudioSupported() {
  return kHasPartialAudioFramesSupport;
}

bool IsAudioPassthroughUsed(const SbPlayerTestConfig& config) {
  const char* audio_dmp_filename = config.audio_filename;
  SbMediaAudioCodec audio_codec = kSbMediaAudioCodecNone;
  if (audio_dmp_filename && strlen(audio_dmp_filename) > 0) {
    VideoDmpReader audio_dmp_reader(audio_dmp_filename,
                                    VideoDmpReader::kEnableReadOnDemand);
    audio_codec = audio_dmp_reader.audio_codec();
  }
  return audio_codec == kSbMediaAudioCodecAc3 ||
         audio_codec == kSbMediaAudioCodecEac3;
}

}  // namespace nplb
}  // namespace starboard
