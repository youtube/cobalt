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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_DECODER_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_DECODER_INTERNAL_H_

#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/closure.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// This class decodes encoded audio stream into playable audio data.
class AudioDecoder {
 public:
  typedef ::starboard::shared::starboard::player::Closure Closure;
  typedef ::starboard::shared::starboard::player::DecodedAudio DecodedAudio;
  typedef ::starboard::shared::starboard::player::InputBuffer InputBuffer;

  virtual ~AudioDecoder() {}

  // Whenever the decoder produces a new output, it calls |output_cb| once and
  // exactly once.  This notify the user to call Read() to acquire the next
  // output.  The user is free to not call Read() immediately but it can expect
  // that a further call of Read() returns valid output until Reset() is called.
  // Note that |output_cb| is always called asynchronously on the calling job
  // queue.
  virtual void Initialize(const Closure& output_cb,
                          const Closure& error_cb) = 0;

  // Decode the encoded audio data stored in |input_buffer|.  Whenever the input
  // is consumed and the decoder is ready to accept a new input, it calls
  // |consumed_cb|.
  // Note that |consumed_cb| is always called asynchronously on the calling job
  // queue.
  virtual void Decode(const scoped_refptr<InputBuffer>& input_buffer,
                      const Closure& consumed_cb) = 0;

  // Notice the object that there is no more input data unless Reset() is
  // called.
  virtual void WriteEndOfStream() = 0;

  // Try to read the next decoded audio buffer.  If the audio stream reaches EOS
  // and there is no more decoded audio available, it returns an EOS buffer.  It
  // should only be called when |output_cb| is called and will always return a
  // valid buffer containing audio data or signals and EOS.
  // Note that there may not be a one-to-one relationship between the decoded
  // audio and the input data passed in via Decode().  The decoder may break or
  // combine multiple decoded audio access units into one.  The implementation
  // has to ensure that the particular resampler can handle such combined access
  // units as input.
  virtual scoped_refptr<DecodedAudio> Read() = 0;

  // Clear any cached buffer of the codec and reset the state of the codec. This
  // function will be called during seek to ensure that the left over data from
  // from previous buffers are cleared.
  virtual void Reset() = 0;

  // Return the sample type of the decoded pcm data.
  virtual SbMediaAudioSampleType GetSampleType() const = 0;

  // Return the storage type of the decoded pcm data.
  virtual SbMediaAudioFrameStorageType GetStorageType() const = 0;

  // Return the sample rate of the incoming audio.  This should be used by the
  // audio renderer as the sample rate of the underlying audio stream can be
  // different than the sample rate stored in the meta data.
  // Note that this function can be called from any thread, as it is called by
  // AudioRendererImpl::GetCurrentTime().
  virtual int GetSamplesPerSecond() const = 0;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_DECODER_INTERNAL_H_
