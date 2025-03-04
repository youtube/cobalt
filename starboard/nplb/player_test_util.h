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

#ifndef STARBOARD_NPLB_PLAYER_TEST_UTIL_H_
#define STARBOARD_NPLB_PLAYER_TEST_UTIL_H_

#include <string>
#include <tuple>
#include <vector>

#include "starboard/configuration_constants.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

struct SbPlayerTestConfig {
  SbPlayerTestConfig(const char* audio_filename,
                     const char* video_filename,
                     SbPlayerOutputMode output_mode,
                     const char* key_system)
      : audio_filename(audio_filename),
        video_filename(video_filename),
        output_mode(output_mode),
        key_system(key_system),
        max_video_capabilities("") {}
  SbPlayerTestConfig(const char* audio_filename,
                     const char* video_filename,
                     SbPlayerOutputMode output_mode,
                     const char* key_system,
                     const char* max_video_capabilities)
      : audio_filename(audio_filename),
        video_filename(video_filename),
        output_mode(output_mode),
        key_system(key_system),
        max_video_capabilities(max_video_capabilities) {}
  const char* audio_filename;
  const char* video_filename;
  SbPlayerOutputMode output_mode;
  const char* key_system;
  const char* max_video_capabilities;
};

std::vector<const char*> GetStereoAudioTestFiles();
std::vector<const char*> GetVideoTestFiles();
std::vector<SbPlayerOutputMode> GetPlayerOutputModes();
std::vector<const char*> GetKeySystems();
std::vector<SbPlayerTestConfig> GetAllPlayerTestConfigs();

std::string GetSbPlayerTestConfigName(
    ::testing::TestParamInfo<SbPlayerTestConfig> info);

void SkipTestIfNotSupported(const SbPlayerTestConfig& config);

void DummyDeallocateSampleFunc(SbPlayer player,
                               void* context,
                               const void* sample_buffer);

void DummyDecoderStatusFunc(SbPlayer player,
                            void* context,
                            SbMediaType type,
                            SbPlayerDecoderState state,
                            int ticket);

void DummyPlayerStatusFunc(SbPlayer player,
                           void* context,
                           SbPlayerState state,
                           int ticket);

void DummyErrorFunc(SbPlayer player,
                    void* context,
                    SbPlayerError error,
                    const char* message);

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
    SbDecodeTargetGraphicsContextProvider* context_provider);

void CallSbPlayerWriteSamples(
    SbPlayer player,
    SbMediaType sample_type,
    shared::starboard::player::video_dmp::VideoDmpReader* dmp_reader,
    int start_index,
    int number_of_samples_to_write,
    int64_t timestamp_offset = 0,
    const std::vector<int64_t>& discarded_durations_from_front = {},
    const std::vector<int64_t>& discarded_durations_from_back = {});

bool IsOutputModeSupported(SbPlayerOutputMode output_mode,
                           SbMediaAudioCodec audio_codec,
                           SbMediaVideoCodec video_codec,
                           const char* key_system = "");

bool IsPartialAudioSupported();

bool IsAudioPassthroughUsed(const SbPlayerTestConfig& config);

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_PLAYER_TEST_UTIL_H_
