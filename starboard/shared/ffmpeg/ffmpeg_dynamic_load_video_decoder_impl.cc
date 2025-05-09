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

// This file contains the creation of the specialized VideoDecoderImpl object
// corresponding to the version of the dynamically loaded ffmpeg library.

#include "starboard/shared/ffmpeg/ffmpeg_video_decoder.h"

#include "starboard/log.h"
#include "starboard/player.h"
#include "starboard/shared/ffmpeg/ffmpeg_dispatch.h"
#include "starboard/shared/ffmpeg/ffmpeg_video_decoder_impl_interface.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

// static
std::unique_ptr<VideoDecoder> VideoDecoder::Create(
    SbMediaVideoCodec video_codec,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider*
        decode_target_graphics_context_provider) {
  FFMPEGDispatch* ffmpeg = FFMPEGDispatch::GetInstance();
  if (!ffmpeg || !ffmpeg->is_valid()) {
    return nullptr;
  }

  switch (ffmpeg->specialization_version()) {
    case 540:
      return VideoDecoderImpl<540>::Create(
          video_codec, output_mode, decode_target_graphics_context_provider);
    case 550:
    case 560:
      return VideoDecoderImpl<560>::Create(
          video_codec, output_mode, decode_target_graphics_context_provider);
    case 571:
      return VideoDecoderImpl<571>::Create(
          video_codec, output_mode, decode_target_graphics_context_provider);
    case 581:
      return VideoDecoderImpl<581>::Create(
          video_codec, output_mode, decode_target_graphics_context_provider);
    case 591:
      return VideoDecoderImpl<591>::Create(
          video_codec, output_mode, decode_target_graphics_context_provider);
    case 601:
      return VideoDecoderImpl<601>::Create(
          video_codec, output_mode, decode_target_graphics_context_provider);
    case 611:
      return VideoDecoderImpl<611>::Create(
          video_codec, output_mode, decode_target_graphics_context_provider);
    default:
      // Go to next step.
      break;
  }
  SB_LOG(WARNING) << "Unsupported FFMPEG version "
                  << ffmpeg->specialization_version();
  return nullptr;
}
}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
