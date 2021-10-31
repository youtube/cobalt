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
#include "starboard/common/string.h"
#include "starboard/directory.h"
#include "starboard/nplb/player_creation_param_helpers.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/testing/fake_graphics_context_provider.h"

namespace starboard {
namespace nplb {

namespace {

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
        if (IsOutputModeSupported(output_mode, kSbMediaVideoCodecNone)) {
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
      if (IsOutputModeSupported(output_mode, dmp_reader.video_codec())) {
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
    const SbMediaAudioSampleInfo* audio_sample_info,
    const char* max_video_capabilities,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    void* context,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider* context_provider) {
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

  if (audio_sample_info) {
    SB_CHECK(audio_sample_info->codec == audio_codec);
  } else {
    SB_CHECK(audio_codec == kSbMediaAudioCodecNone);
  }

  SbPlayerCreationParam creation_param =
      nplb::CreatePlayerCreationParam(audio_codec, video_codec);
  if (audio_sample_info) {
    creation_param.audio_sample_info = *audio_sample_info;
  }
  creation_param.drm_system = drm_system;
  creation_param.output_mode = output_mode;
  creation_param.video_sample_info.max_video_capabilities =
      max_video_capabilities;

  return SbPlayerCreate(window, &creation_param, sample_deallocate_func,
                        decoder_status_func, player_status_func,
                        player_error_func, context, context_provider);

#else  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

  return SbPlayerCreate(
      window, video_codec, audio_codec, kSbDrmSystemInvalid, audio_sample_info,
      max_video_capabilities,
      sample_deallocate_func, decoder_status_func, player_status_func,
      player_error_func, context, output_mode, context_provider);

#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
}

bool IsOutputModeSupported(SbPlayerOutputMode output_mode,
                           SbMediaVideoCodec codec) {
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  SbPlayerCreationParam creation_param =
      CreatePlayerCreationParam(kSbMediaAudioCodecNone, codec);
  creation_param.output_mode = output_mode;
  return SbPlayerGetPreferredOutputMode(&creation_param) == output_mode;
#else   // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  return SbPlayerOutputModeSupported(output_mode, codec, kSbDrmSystemInvalid);
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
}

}  // namespace nplb
}  // namespace starboard
