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

#ifndef COBALT_MEDIA_CAPTURE_ENCODERS_FLAC_AUDIO_ENCODER_H_
#define COBALT_MEDIA_CAPTURE_ENCODERS_FLAC_AUDIO_ENCODER_H_

#include <memory>
#include <string>

#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "cobalt/media_capture/encoders/audio_encoder.h"
#include "cobalt/speech/audio_encoder_flac.h"

namespace cobalt {
namespace media_capture {
namespace encoders {

class FlacAudioEncoder : public AudioEncoder {
 public:
  static bool IsFlacMIMEType(const base::StringPiece& mime_type);

  FlacAudioEncoder();
  ~FlacAudioEncoder() override;

  // Called from the thread the object is constructed on (usually
  // a dedicated audio thread).
  void Encode(const AudioBus& audio_bus,
              base::TimeTicks reference_time) override;

  // This can be called from any thread
  void Finish(base::TimeTicks reference_time) override;

  // Called from the main web module thread.
  std::string GetMimeType() const override;

  int64 GetEstimatedOutputBitsPerSecond(
      const media_stream::AudioParameters& params) const override;

  void OnSetFormat(const media_stream::AudioParameters& params) override;

 private:
  // These functions are called on the encoder thread.
  void CreateEncoder(const media_stream::AudioParameters& params);
  void DestroyEncoder();
  void DoEncode(std::unique_ptr<AudioBus> audio_bus,
                base::TimeTicks reference_time);
  void DoFinish(base::TimeTicks reference_time);

  // Encoder thread
  base::Thread thread_;
  std::unique_ptr<speech::AudioEncoderFlac> flac_encoder_;
};

}  // namespace encoders
}  // namespace media_capture
}  // namespace cobalt

#endif  // COBALT_MEDIA_CAPTURE_ENCODERS_FLAC_AUDIO_ENCODER_H_
