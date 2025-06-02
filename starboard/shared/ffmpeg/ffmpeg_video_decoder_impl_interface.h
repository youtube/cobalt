// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_FFMPEG_FFMPEG_VIDEO_DECODER_IMPL_INTERFACE_H_
#define STARBOARD_SHARED_FFMPEG_FFMPEG_VIDEO_DECODER_IMPL_INTERFACE_H_

#include "starboard/media.h"
#include "starboard/shared/ffmpeg/ffmpeg_video_decoder.h"
#include "starboard/shared/internal_only.h"

namespace starboard::shared::ffmpeg {

// For each version V that is supported, there will be an explicit
// specialization of the VideoDecoder class.
template <int V>
class VideoDecoderImpl : public VideoDecoder {
 public:
  static VideoDecoder* Create(SbMediaVideoCodec video_codec,
                              SbPlayerOutputMode output_mode,
                              SbDecodeTargetGraphicsContextProvider*
                                  decode_target_graphics_context_provider);
};

}  // namespace starboard::shared::ffmpeg

#endif  // STARBOARD_SHARED_FFMPEG_FFMPEG_VIDEO_DECODER_IMPL_INTERFACE_H_
