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

#ifndef COBALT_SPEECH_AUDIO_ENCODER_FLAC_H_
#define COBALT_SPEECH_AUDIO_ENCODER_FLAC_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "cobalt/media/base/audio_bus.h"
#include "third_party/flac/include/FLAC/stream_encoder.h"

namespace cobalt {
namespace speech {

// Encode raw audio to using FLAC codec.
class AudioEncoderFlac {
 public:
  typedef media::AudioBus AudioBus;

  explicit AudioEncoderFlac(int sample_rate);
  ~AudioEncoderFlac();

  // Encode raw audio data.
  void Encode(const AudioBus* audio_bus);
  // Finish encoding.
  void Finish();

  // Returns mime Type of audio data.
  std::string GetMimeType() const;
  // Returns and clears the available encoded audio data.
  std::string GetAndClearAvailableEncodedData();

 private:
  // FLAC encoder callback.
  static FLAC__StreamEncoderWriteStatus WriteCallback(
      const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[],
      size_t bytes, unsigned int samples, unsigned int current_frame,
      void* client_data);

  THREAD_CHECKER(thread_checker_);
  // FLAC encoder.
  FLAC__StreamEncoder* encoder_;
  // Cached encoded data.
  std::string encoded_data_;
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_AUDIO_ENCODER_FLAC_H_
