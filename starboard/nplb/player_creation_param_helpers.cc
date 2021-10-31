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

#include "starboard/nplb/player_creation_param_helpers.h"

#include "starboard/common/log.h"

namespace starboard {
namespace nplb {

SbMediaAudioSampleInfo CreateAudioSampleInfo(SbMediaAudioCodec codec) {
  SbMediaAudioSampleInfo audio_sample_info = {};

  audio_sample_info.codec = codec;

#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  audio_sample_info.mime = "";
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

  switch (codec) {
    case kSbMediaAudioCodecNone:
      break;
    case kSbMediaAudioCodecAac: {
      static const uint8_t kAacAudioSpecificConfig[16] = {18, 16};

      audio_sample_info.format_tag = 0xff;
      audio_sample_info.number_of_channels = 2;
      audio_sample_info.samples_per_second = 44100;
      audio_sample_info.block_alignment = 4;
      audio_sample_info.bits_per_sample = 16;
      audio_sample_info.audio_specific_config = kAacAudioSpecificConfig;
      audio_sample_info.audio_specific_config_size =
          sizeof(kAacAudioSpecificConfig);
      audio_sample_info.average_bytes_per_second =
          audio_sample_info.samples_per_second *
          audio_sample_info.number_of_channels *
          audio_sample_info.bits_per_sample / 8;
      break;
    }
    case kSbMediaAudioCodecAc3:
    case kSbMediaAudioCodecEac3: {
      audio_sample_info.format_tag = 0xff;
      audio_sample_info.number_of_channels = 6;
      audio_sample_info.samples_per_second = 48000;
      audio_sample_info.block_alignment = 4;
      audio_sample_info.bits_per_sample = 16;
      audio_sample_info.audio_specific_config = nullptr;
      audio_sample_info.audio_specific_config_size = 0;
      audio_sample_info.average_bytes_per_second =
          audio_sample_info.samples_per_second *
          audio_sample_info.number_of_channels *
          audio_sample_info.bits_per_sample / 8;
      break;
    }
    case kSbMediaAudioCodecOpus: {
      static const uint8_t kOpusAudioSpecificConfig[19] = {
          79, 112, 117, 115, 72, 101, 97, 100, 1, 2, 56, 1, 128, 187};

      audio_sample_info.format_tag = 0xff;
      audio_sample_info.number_of_channels = 2;
      audio_sample_info.samples_per_second = 48000;
      audio_sample_info.block_alignment = 4;
      audio_sample_info.bits_per_sample = 32;
      audio_sample_info.audio_specific_config = kOpusAudioSpecificConfig;
      audio_sample_info.audio_specific_config_size =
          sizeof(kOpusAudioSpecificConfig);
      audio_sample_info.average_bytes_per_second =
          audio_sample_info.samples_per_second *
          audio_sample_info.number_of_channels *
          audio_sample_info.bits_per_sample / 8;
      break;
    }
    case kSbMediaAudioCodecVorbis: {
      // Note that unlike the configuration of the other formats, the following
      // configuration is made up, instead of taking from a real input.
      audio_sample_info.format_tag = 0xff;
      audio_sample_info.number_of_channels = 2;
      audio_sample_info.samples_per_second = 48000;
      audio_sample_info.block_alignment = 4;
      audio_sample_info.bits_per_sample = 16;
      audio_sample_info.audio_specific_config = nullptr;
      audio_sample_info.audio_specific_config_size = 0;
      audio_sample_info.average_bytes_per_second =
          audio_sample_info.samples_per_second *
          audio_sample_info.number_of_channels *
          audio_sample_info.bits_per_sample / 8;
      break;
    }
  }
  return audio_sample_info;
}

SbMediaVideoSampleInfo CreateVideoSampleInfo(SbMediaVideoCodec codec) {
  SbMediaVideoSampleInfo video_sample_info = {};

  video_sample_info.codec = codec;

#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  video_sample_info.mime = "";
  video_sample_info.max_video_capabilities = "";
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

  video_sample_info.color_metadata.primaries = kSbMediaPrimaryIdBt709;
  video_sample_info.color_metadata.transfer = kSbMediaTransferIdBt709;
  video_sample_info.color_metadata.matrix = kSbMediaMatrixIdBt709;
  video_sample_info.color_metadata.range = kSbMediaRangeIdLimited;

  video_sample_info.frame_width = 1920;
  video_sample_info.frame_height = 1080;

  return video_sample_info;
}

#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

SbPlayerCreationParam CreatePlayerCreationParam(SbMediaAudioCodec audio_codec,
                                                SbMediaVideoCodec video_codec) {
  SbPlayerCreationParam creation_param = {};

  creation_param.drm_system = kSbDrmSystemInvalid;
  creation_param.audio_sample_info = CreateAudioSampleInfo(audio_codec);
  creation_param.video_sample_info = CreateVideoSampleInfo(video_codec);
  creation_param.output_mode = kSbPlayerOutputModeInvalid;

  return creation_param;
}

#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

}  // namespace nplb
}  // namespace starboard
