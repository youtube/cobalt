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
#include "starboard/extension/enhanced_audio.h"
#include "starboard/nplb/drm_helpers.h"
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

std::vector<SbPlayerTestConfig> GetSupportedSbPlayerTestConfigs(
    const char* key_system) {
  SB_DCHECK(key_system);

  const char* kEmptyName = NULL;

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

}  // namespace nplb
}  // namespace starboard
