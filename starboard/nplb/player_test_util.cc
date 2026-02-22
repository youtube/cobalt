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
#include <cstring>
#include <functional>

#include "starboard/audio_sink.h"
#include "starboard/common/check_op.h"
#include "starboard/common/string.h"
#include "starboard/nplb/drm_helpers.h"
#include "starboard/nplb/maximum_player_configuration_explorer.h"
#include "starboard/nplb/player_creation_param_helpers.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {

namespace {

using ::starboard::AudioSampleInfo;
using ::starboard::VideoDmpReader;
using ::starboard::VideoSampleInfo;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

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

}  // namespace

std::vector<const char*> GetStereoAudioTestFiles() {
  static std::vector<const char*> stereo_audio_test_files;
  if (!stereo_audio_test_files.empty()) {
    return stereo_audio_test_files;
  }

  for (const char* audio_filename : kAudioTestFiles) {
    if (std::strstr(audio_filename, "stereo") != nullptr) {
      stereo_audio_test_files.emplace_back(audio_filename);
    }
  }
  return stereo_audio_test_files;
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

std::vector<SbPlayerTestConfig> GetAllPlayerTestConfigs() {
  const char* kEmptyName = nullptr;
  static std::vector<SbPlayerTestConfig> test_configs;

  if (!test_configs.empty()) {
    return test_configs;
  }

  for (auto key_system : kKeySystems) {
    for (auto output_mode : kOutputModes) {
      // Add audio only tests.
      for (auto audio_filename : kAudioTestFiles) {
        test_configs.emplace_back(audio_filename, kEmptyName, output_mode,
                                  key_system);
      }
      for (auto audio_filename : kAudioOnlyTestFiles) {
        test_configs.emplace_back(audio_filename, kEmptyName, output_mode,
                                  key_system);
      }
      // Add video only tests.
      for (auto video_filename : kVideoTestFiles) {
        test_configs.emplace_back(kEmptyName, video_filename, output_mode,
                                  key_system);
      }
      // Add audio and video tests.
      for (auto audio_filename : kAudioTestFiles) {
        for (auto video_filename : kVideoTestFiles) {
          test_configs.emplace_back(audio_filename, video_filename, output_mode,
                                    key_system);
        }
      }
    }
  }

  SB_DCHECK(!test_configs.empty());
  return test_configs;
}

std::string GetSbPlayerTestConfigName(
    ::testing::TestParamInfo<SbPlayerTestConfig> info) {
  const SbPlayerTestConfig& config = info.param;
  const char* audio_filename = config.audio_filename;
  const char* video_filename = config.video_filename;
  const SbPlayerOutputMode output_mode = config.output_mode;
  const char* key_system = config.key_system;
  std::string name(starboard::FormatString(
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

void SkipTestIfNotSupported(const SbPlayerTestConfig& config) {
  SbMediaAudioCodec audio_codec = kSbMediaAudioCodecNone;
  if (config.audio_filename && strlen(config.audio_filename) > 0) {
    VideoDmpReader dmp_reader(config.audio_filename,
                              VideoDmpReader::kEnableReadOnDemand);
    SB_DCHECK_GT(dmp_reader.number_of_audio_buffers(), static_cast<size_t>(0));
    if (!SbMediaCanPlayMimeAndKeySystem(dmp_reader.audio_mime_type().c_str(),
                                        config.key_system)) {
      GTEST_SKIP() << "Unsupported audio config.";
    }
    audio_codec = dmp_reader.audio_codec();
  }

  SbMediaVideoCodec video_codec = kSbMediaVideoCodecNone;
  if (config.video_filename && strlen(config.video_filename) > 0) {
    VideoDmpReader dmp_reader(config.video_filename,
                              VideoDmpReader::kEnableReadOnDemand);
    SB_DCHECK_GT(dmp_reader.number_of_video_buffers(), static_cast<size_t>(0));
    if (!SbMediaCanPlayMimeAndKeySystem(dmp_reader.video_mime_type().c_str(),
                                        config.key_system)) {
      GTEST_SKIP() << "Unsupported video config.";
    }
    video_codec = dmp_reader.video_codec();
  }

  if (!IsOutputModeSupported(config.output_mode, audio_codec, video_codec,
                             config.key_system)) {
    GTEST_SKIP() << "Unsupported output mode.";
  }
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
    const starboard::AudioStreamInfo* audio_stream_info,
    const char* max_video_capabilities,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    void* context,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider* context_provider) {
  if (audio_stream_info) {
    SB_CHECK_EQ(audio_stream_info->codec, audio_codec);
  } else {
    SB_CHECK_EQ(audio_codec, kSbMediaAudioCodecNone);
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
    starboard::VideoDmpReader* dmp_reader,
    int start_index,
    int number_of_samples_to_write,
    int64_t timestamp_offset,
    const std::vector<int64_t>& discarded_durations_from_front,
    const std::vector<int64_t>& discarded_durations_from_back) {
  SB_DCHECK_GE(start_index, 0);
  SB_DCHECK_GT(number_of_samples_to_write, 0);

  if (sample_type == kSbMediaTypeAudio) {
    SB_DCHECK(discarded_durations_from_front.empty() ||
              discarded_durations_from_front.size() ==
                  static_cast<size_t>(number_of_samples_to_write));
    SB_DCHECK_EQ(discarded_durations_from_front.size(),
                 discarded_durations_from_back.size());
  } else {
    SB_DCHECK_EQ(sample_type, kSbMediaTypeVideo);
    SB_DCHECK(discarded_durations_from_front.empty());
    SB_DCHECK(discarded_durations_from_back.empty());
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
