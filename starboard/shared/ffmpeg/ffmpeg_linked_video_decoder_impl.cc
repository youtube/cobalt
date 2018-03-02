// Copyright 2018 Google Inc. All Rights Reserved.
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
// corresponding to the version of the linked ffmpeg library.

#include "starboard/shared/ffmpeg/ffmpeg_video_decoder.h"

#include "starboard/memory.h"
#include "starboard/player.h"
#include "starboard/shared/ffmpeg/ffmpeg_common.h"
#include "starboard/shared/ffmpeg/ffmpeg_dispatch.h"
#include "starboard/shared/ffmpeg/ffmpeg_video_decoder_impl.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

// static
VideoDecoder* VideoDecoder::Create(
    SbMediaVideoCodec video_codec,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider*
        decode_target_graphics_context_provider) {
  FFMPEGDispatch* ffmpeg = FFMPEGDispatch::GetInstance();
  SB_DCHECK(ffmpeg && ffmpeg->is_valid());
  SB_DCHECK(FFMPEG == ffmpeg->specialization_version());

  return VideoDecoderImpl<FFMPEG>::Create(
      video_codec, output_mode, decode_target_graphics_context_provider);
}
}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
