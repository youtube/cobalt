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

// This file contains the creation of the specialized AudioDecoderImpl object
// corresponding to the version of the dynamically loaded ffmpeg library.

#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder.h"

#include "starboard/memory.h"
#include "starboard/player.h"
#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder_impl.h"
#include "starboard/shared/ffmpeg/ffmpeg_dispatch.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

// static
AudioDecoder* AudioDecoder::Create(SbMediaAudioCodec audio_codec,
                                   const SbMediaAudioHeader& audio_header) {
  FFMPEGDispatch* ffmpeg = FFMPEGDispatch::GetInstance();
  if (!ffmpeg || !ffmpeg->is_valid()) {
    SB_NOTREACHED();
    return NULL;
  }

  AudioDecoder* audio_decoder = NULL;
  switch (ffmpeg->specialization_version()) {
    case 540:
      audio_decoder = AudioDecoderImpl<540>::Create(audio_codec, audio_header);
      break;
    case 550:
    case 560:
      audio_decoder = AudioDecoderImpl<560>::Create(audio_codec, audio_header);
      break;
    case 571:
      audio_decoder = AudioDecoderImpl<571>::Create(audio_codec, audio_header);
      break;
    default:
      SB_NOTREACHED() << "Unsupported FFMPEG specialization " << std::hex
                      << ffmpeg->specialization_version();
      break;
  }
  return audio_decoder;
}

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
