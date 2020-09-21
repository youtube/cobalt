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

#ifndef COBALT_MEDIA_CAPTURE_ENCODERS_LINEAR16_AUDIO_ENCODER_H_
#define COBALT_MEDIA_CAPTURE_ENCODERS_LINEAR16_AUDIO_ENCODER_H_

#include <string>

#include "base/strings/string_piece.h"
#include "cobalt/media_capture/encoders/audio_encoder.h"

namespace cobalt {
namespace media_capture {
namespace encoders {

class Linear16AudioEncoder : public AudioEncoder {
 public:
  static bool IsLinear16MIMEType(const base::StringPiece& mime_type);

  Linear16AudioEncoder() = default;
  void Encode(const AudioBus& audio_bus,
              base::TimeTicks reference_time) override;
  void Finish(base::TimeTicks reference_time) override;
  std::string GetMimeType() const override;

  int64 GetEstimatedOutputBitsPerSecond(
      const media_stream::AudioParameters& params) const override;

  void OnSetFormat(const media_stream::AudioParameters& params) override;
};

}  // namespace encoders
}  // namespace media_capture
}  // namespace cobalt

#endif  // COBALT_MEDIA_CAPTURE_ENCODERS_LINEAR16_AUDIO_ENCODER_H_
