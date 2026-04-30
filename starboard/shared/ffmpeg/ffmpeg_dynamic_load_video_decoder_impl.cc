// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

// clang-format off
#include "starboard/shared/ffmpeg/ffmpeg_video_decoder.h"
// clang-format on

// This file contains the creation of the specialized VideoDecoderImpl object
// corresponding to the version of the dynamically loaded ffmpeg library.

#include <memory>

#include "starboard/log.h"
#include "starboard/player.h"
#include "starboard/shared/ffmpeg/ffmpeg_dispatch.h"
#include "starboard/shared/ffmpeg/ffmpeg_video_decoder_impl_interface.h"

namespace starboard {

// static
std::unique_ptr<FfmpegVideoDecoder> FfmpegVideoDecoder::Create(
    SbMediaVideoCodec video_codec,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider*
        decode_target_graphics_context_provider) {
  FFMPEGDispatch* ffmpeg = FFMPEGDispatch::GetInstance();
  if (!ffmpeg) {
    return nullptr;
  }

  switch (ffmpeg->specialization_version()) {
    case 540:
      return FfmpegVideoDecoderImpl<540>::Create(
          video_codec, output_mode, decode_target_graphics_context_provider);
    case 550:
    case 560:
      return FfmpegVideoDecoderImpl<560>::Create(
          video_codec, output_mode, decode_target_graphics_context_provider);
    case 571:
      return FfmpegVideoDecoderImpl<571>::Create(
          video_codec, output_mode, decode_target_graphics_context_provider);
    case 581:
      return FfmpegVideoDecoderImpl<581>::Create(
          video_codec, output_mode, decode_target_graphics_context_provider);
    case 591:
      return FfmpegVideoDecoderImpl<591>::Create(
          video_codec, output_mode, decode_target_graphics_context_provider);
    case 601:
      return FfmpegVideoDecoderImpl<601>::Create(
          video_codec, output_mode, decode_target_graphics_context_provider);
    case 611:
      return FfmpegVideoDecoderImpl<611>::Create(
          video_codec, output_mode, decode_target_graphics_context_provider);
    default:
      SB_LOG(WARNING) << "Unsupported FFMPEG version "
                      << ffmpeg->specialization_version();
      break;
  }
  return nullptr;
}

}  // namespace starboard
