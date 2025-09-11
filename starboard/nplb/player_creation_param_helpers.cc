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
#include "starboard/shared/starboard/player/video_dmp_reader.h"

namespace starboard {
namespace nplb {
namespace {

using shared::starboard::media::AudioStreamInfo;
using shared::starboard::media::VideoStreamInfo;
using shared::starboard::player::video_dmp::VideoDmpReader;

}  // namespace

AudioStreamInfo CreateAudioStreamInfo(SbMediaAudioCodec codec) {
  AudioStreamInfo audio_stream_info = {};

  audio_stream_info.codec = codec;
  audio_stream_info.mime = "";

  switch (codec) {
    case kSbMediaAudioCodecNone:
      break;
    case kSbMediaAudioCodecAac: {
      static const uint8_t kAacAudioSpecificConfig[16] = {18, 16};

      audio_stream_info.number_of_channels = 2;
      audio_stream_info.samples_per_second = 44100;
      audio_stream_info.bits_per_sample = 16;
      audio_stream_info.audio_specific_config.assign(
          kAacAudioSpecificConfig,
          kAacAudioSpecificConfig + sizeof(kAacAudioSpecificConfig));
      break;
    }
    case kSbMediaAudioCodecAc3:
    case kSbMediaAudioCodecEac3: {
      audio_stream_info.number_of_channels = 6;
      audio_stream_info.samples_per_second = 48000;
      audio_stream_info.bits_per_sample = 16;
      break;
    }
    case kSbMediaAudioCodecOpus: {
      static const uint8_t kOpusAudioSpecificConfig[19] = {
          79, 112, 117, 115, 72, 101, 97, 100, 1, 2, 56, 1, 128, 187};

      audio_stream_info.number_of_channels = 2;
      audio_stream_info.samples_per_second = 48000;
      audio_stream_info.bits_per_sample = 32;
      audio_stream_info.audio_specific_config.assign(
          kOpusAudioSpecificConfig,
          kOpusAudioSpecificConfig + sizeof(kOpusAudioSpecificConfig));
      break;
    }
    case kSbMediaAudioCodecVorbis: {
      // Note that unlike the configuration of the other formats, the following
      // configuration is made up, instead of taking from a real input.
      audio_stream_info.number_of_channels = 2;
      audio_stream_info.samples_per_second = 48000;
      audio_stream_info.bits_per_sample = 16;
      break;
    }
    case kSbMediaAudioCodecMp3: {
      audio_stream_info.number_of_channels = 2;
      audio_stream_info.samples_per_second = 44100;
      audio_stream_info.bits_per_sample = 16;
      break;
    }
    case kSbMediaAudioCodecFlac: {
      audio_stream_info.number_of_channels = 2;
      audio_stream_info.samples_per_second = 44100;
      audio_stream_info.bits_per_sample = 16;
      break;
    }
    case kSbMediaAudioCodecPcm: {
      audio_stream_info.number_of_channels = 2;
      audio_stream_info.samples_per_second = 44100;
      audio_stream_info.bits_per_sample = 32;
      break;
    }
    case kSbMediaAudioCodecIamf: {
      audio_stream_info.number_of_channels = 2;
      audio_stream_info.samples_per_second = 48000;
      audio_stream_info.bits_per_sample = 32;
      break;
    }
  }
  return audio_stream_info;
}

VideoStreamInfo CreateVideoStreamInfo(SbMediaVideoCodec codec) {
  VideoStreamInfo video_stream_info;

  video_stream_info.codec = codec;

  video_stream_info.mime = "";
  video_stream_info.max_video_capabilities = "";
  video_stream_info.color_metadata.bits_per_channel = 8;
  video_stream_info.color_metadata.primaries = kSbMediaPrimaryIdBt709;
  video_stream_info.color_metadata.transfer = kSbMediaTransferIdBt709;
  video_stream_info.color_metadata.matrix = kSbMediaMatrixIdBt709;
  video_stream_info.color_metadata.range = kSbMediaRangeIdLimited;

  video_stream_info.frame_width = 1920;
  video_stream_info.frame_height = 1080;

  return video_stream_info;
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
    creation_param.video_stream_info = dmp_reader.video_stream_info();
    creation_param.video_stream_info.max_video_capabilities =
        config.max_video_capabilities;
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
}  // namespace starboard
