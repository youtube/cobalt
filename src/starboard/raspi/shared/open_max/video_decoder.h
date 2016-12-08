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

#ifndef STARBOARD_RASPI_SHARED_OPEN_MAX_VIDEO_DECODER_H_
#define STARBOARD_RASPI_SHARED_OPEN_MAX_VIDEO_DECODER_H_

#include "starboard/media.h"
#include "starboard/raspi/shared/open_max/open_max_video_decode_component.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"

namespace starboard {
namespace raspi {
namespace shared {
namespace open_max {

class VideoDecoder
    : public starboard::shared::starboard::player::filter::VideoDecoder {
 public:
  typedef starboard::shared::starboard::player::InputBuffer InputBuffer;

  explicit VideoDecoder(SbMediaVideoCodec video_codec);
  ~VideoDecoder() SB_OVERRIDE;

  void SetHost(Host* host) SB_OVERRIDE;
  void WriteInputBuffer(const InputBuffer& input_buffer) SB_OVERRIDE;
  void WriteEndOfStream() SB_OVERRIDE;
  void Reset() SB_OVERRIDE;

 private:
  OpenMaxVideoDecodeComponent component_;

  // These variables will be initialized inside ctor or SetHost() and will not
  // be changed during the life time of this class.
  Host* host_;

  bool stream_ended_;
};

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard

#endif  // STARBOARD_RASPI_SHARED_OPEN_MAX_VIDEO_DECODER_H_
