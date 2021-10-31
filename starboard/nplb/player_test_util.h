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

namespace starboard {
namespace nplb {

typedef std::tuple<const char* /* audio_filename */,
                   const char* /* video_filename */,
                   SbPlayerOutputMode /* output_mode */>
    SbPlayerTestConfig;

std::vector<SbPlayerTestConfig> GetSupportedSbPlayerTestConfigs();

std::string ResolveTestFileName(const char* filename);

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
    const SbMediaAudioSampleInfo* audio_sample_info,
    const char* max_video_capabilities,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    void* context,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider* context_provider);

bool IsOutputModeSupported(SbPlayerOutputMode output_mode,
                           SbMediaVideoCodec codec);

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_PLAYER_TEST_UTIL_H_
