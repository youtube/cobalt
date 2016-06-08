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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_AUDIO_DECODER_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_AUDIO_DECODER_INTERNAL_H_

#include <vector>

#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

// This class decodes encoded audio stream into playable audio data.
class AudioDecoder {
 public:
  virtual ~AudioDecoder() {}

  // Decode the encoded audio data stored in |input_buffer| and store the
  // result in |output|.  After this call, the VideoDecoder instance owns
  // |input_buffer| and will delete it once finished using it.
  virtual void Decode(InputBuffer* input_buffer,
                      std::vector<float>* output) = 0;
  // Note that there won't be more input data unless Reset() is called.
  virtual void WriteEndOfStream() = 0;
  // Clear any cached buffer of the codec and reset the state of the codec.
  // This function will be called during seek to ensure that the left over
  // data from previous buffers are cleared.
  virtual void Reset() = 0;

  // Return the sample rate of the incoming audio.  This should be used by the
  // audio renderer as the sample rate of the underlying audio stream can be
  // different than the sample rate stored in the meta data.
  virtual int GetSamplesPerSecond() = 0;

  // Individual implementation has to implement this function to create an
  // audio decoder.
  static AudioDecoder* Create(SbMediaAudioCodec audio_codec,
                              const SbMediaAudioHeader& audio_header);
};

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_AUDIO_DECODER_INTERNAL_H_
