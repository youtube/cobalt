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

#ifndef STARBOARD_SHARED_FFMPEG_FFMPEG_AUDIO_DECODER_H_
#define STARBOARD_SHARED_FFMPEG_FFMPEG_AUDIO_DECODER_H_

#include <vector>

#include "starboard/media.h"
#include "starboard/shared/ffmpeg/ffmpeg_common.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/audio_decoder_internal.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

class AudioDecoder : public starboard::player::AudioDecoder {
 public:
  typedef starboard::player::InputBuffer InputBuffer;

  AudioDecoder(SbMediaAudioCodec audio_codec,
               const SbMediaAudioHeader& audio_header);
  ~AudioDecoder() SB_OVERRIDE;

  void Decode(InputBuffer* input_buffer,
              std::vector<float>* output) SB_OVERRIDE;
  void WriteEndOfStream() SB_OVERRIDE;
  void Reset() SB_OVERRIDE;

 private:
  void InitializeCodec();
  void TeardownCodec();

  AVCodecContext* codec_context_;
  AVFrame* av_frame_;

  bool stream_ended_;
  SbMediaAudioHeader audio_header_;
};

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_FFMPEG_FFMPEG_AUDIO_DECODER_H_
