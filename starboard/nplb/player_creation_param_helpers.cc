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

#include "starboard/shared/starboard/player/video_dmp_reader.h"

namespace nplb {
namespace {

using ::starboard::VideoDmpReader;

}  // namespace

starboard::AudioStreamInfo CreateAudioStreamInfo(SbMediaAudioCodec codec) {
  SbMediaAudioStreamInfo sb_info = {};
  sb_info.codec = codec;
  sb_info.mime = "";

  switch (codec) {
    case kSbMediaAudioCodecNone:
      break;
    case kSbMediaAudioCodecAac: {
      static const uint8_t kAacAudioSpecificConfig[16] = {18, 16};
      sb_info.number_of_channels = 2;
      sb_info.samples_per_second = 44100;
      sb_info.bits_per_sample = 16;
      sb_info.audio_specific_config = kAacAudioSpecificConfig;
      sb_info.audio_specific_config_size = sizeof(kAacAudioSpecificConfig);
      break;
    }
    case kSbMediaAudioCodecAc3:
    case kSbMediaAudioCodecEac3: {
      sb_info.number_of_channels = 6;
      sb_info.samples_per_second = 48000;
      sb_info.bits_per_sample = 16;
      break;
    }
    case kSbMediaAudioCodecOpus: {
      static const uint8_t kOpusAudioSpecificConfig[19] = {
          79, 112, 117, 115, 72, 101, 97, 100, 1, 2, 56, 1, 128, 187};
      sb_info.number_of_channels = 2;
      sb_info.samples_per_second = 48000;
      sb_info.bits_per_sample = 32;
      sb_info.audio_specific_config = kOpusAudioSpecificConfig;
      sb_info.audio_specific_config_size = sizeof(kOpusAudioSpecificConfig);
      break;
    }
    case kSbMediaAudioCodecVorbis: {
      sb_info.number_of_channels = 2;
      sb_info.samples_per_second = 48000;
      sb_info.bits_per_sample = 16;
      break;
    }
    case kSbMediaAudioCodecMp3: {
      sb_info.number_of_channels = 2;
      sb_info.samples_per_second = 44100;
      sb_info.bits_per_sample = 16;
      break;
    }
    case kSbMediaAudioCodecFlac: {
      sb_info.number_of_channels = 2;
      sb_info.samples_per_second = 44100;
      sb_info.bits_per_sample = 16;
      break;
    }
    case kSbMediaAudioCodecPcm: {
      sb_info.number_of_channels = 2;
      sb_info.samples_per_second = 44100;
      sb_info.bits_per_sample = 32;
      break;
    }
    case kSbMediaAudioCodecIamf: {
      sb_info.number_of_channels = 2;
      sb_info.samples_per_second = 48000;
      sb_info.bits_per_sample = 32;
      break;
    }
  }
  return starboard::AudioStreamInfo(sb_info);
}

starboard::VideoStreamInfo CreateVideoStreamInfo(SbMediaVideoCodec codec) {
  SbMediaVideoStreamInfo sb_info = {};
  sb_info.codec = codec;
  sb_info.mime = "";
  sb_info.max_video_capabilities = "";
  sb_info.frame_width = 1920;
  sb_info.frame_height = 1080;
  sb_info.color_metadata.bits_per_channel = 8;
  sb_info.color_metadata.primaries = kSbMediaPrimaryIdBt709;
  sb_info.color_metadata.transfer = kSbMediaTransferIdBt709;
  sb_info.color_metadata.matrix = kSbMediaMatrixIdBt709;
  sb_info.color_metadata.range = kSbMediaRangeIdLimited;

  return starboard::VideoStreamInfo(sb_info);
}

PlayerCreationParam CreatePlayerCreationParam(SbMediaAudioCodec audio_codec,
                                              SbMediaVideoCodec video_codec,
                                              SbPlayerOutputMode output_mode) {
  PlayerCreationParam creation_param;

  creation_param.audio_stream_info = CreateAudioStreamInfo(audio_codec);
  creation_param.video_stream_info = CreateVideoStreamInfo(video_codec);
  creation_param.output_mode = output_mode;

  return creation_param;
}

PlayerCreationParam CreatePlayerCreationParam(
    const SbPlayerTestConfig& config) {
  PlayerCreationParam creation_param;
  if (config.audio_filename) {
    VideoDmpReader dmp_reader(config.audio_filename);
    creation_param.audio_stream_info = dmp_reader.audio_stream_info();
  }
  if (config.video_filename) {
    VideoDmpReader dmp_reader(config.video_filename);
    SbMediaVideoStreamInfo sb_info = {};
    dmp_reader.video_stream_info().ConvertTo(&sb_info);
    sb_info.max_video_capabilities = config.max_video_capabilities;
    creation_param.video_stream_info = starboard::VideoStreamInfo(sb_info);
  }
  creation_param.output_mode = config.output_mode;
  return creation_param;
}

SbPlayerOutputMode GetPreferredOutputMode(
    const PlayerCreationParam& creation_param) {
  SbPlayerCreationParam param = {};
  creation_param.ConvertTo(&param);
  return SbPlayerGetPreferredOutputMode(&param);
}

}  // namespace nplb
