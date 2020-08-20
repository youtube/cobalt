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

#include "cobalt/media_capture/encoders/linear16_audio_encoder.h"

#include <algorithm>
#include <string>

#include "base/strings/string_util.h"

namespace {
// See https://tools.ietf.org/html/rfc2586 for MIME type
const char kLinear16MimeType[] = "audio/L16";
}  // namespace

namespace cobalt {
namespace media_capture {
namespace encoders {

bool Linear16AudioEncoder::IsLinear16MIMEType(
    const base::StringPiece& mime_type) {
  base::StringPiece mime_type_container = mime_type;
  size_t pos = mime_type.find_first_of(';');
  if (pos != base::StringPiece::npos) {
    mime_type_container = base::StringPiece(mime_type.begin(), pos);
  }
  const base::StringPiece linear16_mime_type(kLinear16MimeType);
  auto match_iterator =
      std::search(mime_type_container.begin(), mime_type_container.end(),
                  linear16_mime_type.begin(), linear16_mime_type.end(),
                  base::CaseInsensitiveCompareASCII<char>());
  return match_iterator == mime_type_container.begin();
}

void Linear16AudioEncoder::Encode(const AudioBus& audio_bus,
                                  base::TimeTicks reference_time) {
  DCHECK_EQ(audio_bus.sample_type(), AudioBus::kInt16);
  DCHECK_EQ(audio_bus.channels(), size_t(1));
  auto data = audio_bus.interleaved_data();
  size_t data_size = audio_bus.GetSampleSizeInBytes() * audio_bus.frames();
  PushDataToAllListeners(data, data_size, reference_time);
}

void Linear16AudioEncoder::Finish(base::TimeTicks reference_time) {
  PushDataToAllListeners(nullptr, 0, reference_time);
}

std::string Linear16AudioEncoder::GetMimeType() const {
  return kLinear16MimeType;
}

int64 Linear16AudioEncoder::GetEstimatedOutputBitsPerSecond(
    const media_stream::AudioParameters& params) const {
  DCHECK_EQ(params.bits_per_sample(), 16);
  DCHECK_EQ(params.channel_count(), 1);
  return params.GetBitsPerSecond();
}

void Linear16AudioEncoder::OnSetFormat(
    const media_stream::AudioParameters& params) {
  DCHECK_EQ(params.bits_per_sample(), 16);
  DCHECK_EQ(params.channel_count(), 1);
}

}  // namespace encoders
}  // namespace media_capture
}  // namespace cobalt
