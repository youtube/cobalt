// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_FFMPEG_FFMPEG_AUDIO_DECODER_H_
#define STARBOARD_SHARED_FFMPEG_FFMPEG_AUDIO_DECODER_H_

#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"

namespace starboard::shared::ffmpeg {

class AudioDecoder : public starboard::player::filter::AudioDecoder {
 public:
  typedef starboard::media::AudioStreamInfo AudioStreamInfo;

  // Create an audio decoder for the currently loaded ffmpeg library.
  static AudioDecoder* Create(const AudioStreamInfo& audio_stream_info);
  // Returns true if the audio decoder is initialized successfully.
  virtual bool is_valid() const = 0;
};

}  // namespace starboard::shared::ffmpeg

#endif  // STARBOARD_SHARED_FFMPEG_FFMPEG_AUDIO_DECODER_H_
