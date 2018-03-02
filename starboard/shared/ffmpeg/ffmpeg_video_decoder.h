// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_FFMPEG_FFMPEG_VIDEO_DECODER_H_
#define STARBOARD_SHARED_FFMPEG_FFMPEG_VIDEO_DECODER_H_

#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

class VideoDecoder : public starboard::player::filter::VideoDecoder {
 public:
  // Create a video decoder for the currently loaded ffmpeg library.
  static VideoDecoder* Create(SbMediaVideoCodec video_codec,
                              SbPlayerOutputMode output_mode,
                              SbDecodeTargetGraphicsContextProvider*
                                  decode_target_graphics_context_provider);

  // Returns true if the video decoder is initialized successfully.
  virtual bool is_valid() const = 0;
};

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_FFMPEG_FFMPEG_VIDEO_DECODER_H_
