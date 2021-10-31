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

#include "cobalt/media_capture/encoders/flac_audio_encoder.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_util.h"

namespace {

// See https://tools.ietf.org/html/rfc2586 for MIME type
const char kFlacMimeType[] = "audio/flac";

// This is a guesstimated value. This number should be > 1.
const double kEstimatedFlacCompressionRatio = 1.30;

}  // namespace

namespace cobalt {
namespace media_capture {
namespace encoders {

bool FlacAudioEncoder::IsFlacMIMEType(const base::StringPiece& mime_type) {
  base::StringPiece mime_type_container = mime_type;
  size_t pos = mime_type.find_first_of(';');
  if (pos != base::StringPiece::npos) {
    mime_type_container = base::StringPiece(mime_type.begin(), pos);
  }
  const base::StringPiece flac_mime_type(kFlacMimeType);
  auto match_iterator =
      std::search(mime_type_container.begin(), mime_type_container.end(),
                  flac_mime_type.begin(), flac_mime_type.end(),
                  base::CaseInsensitiveCompareASCII<char>());
  return match_iterator == mime_type_container.begin();
}

FlacAudioEncoder::FlacAudioEncoder() : thread_("FlacEncodingThd") {
  thread_.Start();
}

FlacAudioEncoder::~FlacAudioEncoder() {
  thread_.task_runner()->PostBlockingTask(
      FROM_HERE,
      base::Bind(&FlacAudioEncoder::DestroyEncoder, base::Unretained(this)));
}

void FlacAudioEncoder::Encode(const AudioBus& audio_bus,
                              base::TimeTicks reference_time) {
  std::unique_ptr<AudioBus> audio_bus_copy(
      new AudioBus(audio_bus.channels(), audio_bus.frames(),
                   audio_bus.sample_type(), audio_bus.storage_type()));
  audio_bus_copy->Assign(audio_bus);

  // base::Unretained usage is safe here, since we're posting to a thread that
  // we own.
  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&FlacAudioEncoder::DoEncode, base::Unretained(this),
                            base::Passed(&audio_bus_copy), reference_time));
}

void FlacAudioEncoder::Finish(base::TimeTicks reference_time) {
  thread_.task_runner()->PostBlockingTask(
      FROM_HERE, base::Bind(&FlacAudioEncoder::DoFinish, base::Unretained(this),
                            reference_time));
}

std::string FlacAudioEncoder::GetMimeType() const {
  return flac_encoder_->GetMimeType();
}

int64 FlacAudioEncoder::GetEstimatedOutputBitsPerSecond(
    const media_stream::AudioParameters& params) const {
  DCHECK_EQ(params.bits_per_sample(), 16);
  DCHECK_EQ(params.channel_count(), 1);
  return static_cast<int64>(params.GetBitsPerSecond() /
                            kEstimatedFlacCompressionRatio);
}

void FlacAudioEncoder::OnSetFormat(
    const media_stream::AudioParameters& params) {
  // base::Unretained usage is safe here, since we're posting to a thread that
  // we own.
  thread_.task_runner()->PostTask(FROM_HERE,
                                  base::Bind(&FlacAudioEncoder::CreateEncoder,
                                             base::Unretained(this), params));
}

void FlacAudioEncoder::CreateEncoder(
    const media_stream::AudioParameters& params) {
  flac_encoder_.reset(new speech::AudioEncoderFlac(params.sample_rate()));
}

void FlacAudioEncoder::DestroyEncoder() { flac_encoder_.reset(); }

void FlacAudioEncoder::DoEncode(std::unique_ptr<AudioBus> audio_bus,
                                base::TimeTicks reference_time) {
  DCHECK(flac_encoder_);
  flac_encoder_->Encode(audio_bus.get());
  auto data = flac_encoder_->GetAndClearAvailableEncodedData();
  if (data.empty()) {
    return;
  }
  PushDataToAllListeners(reinterpret_cast<uint8*>(&data[0]), data.size(),
                         reference_time);
}

void FlacAudioEncoder::DoFinish(base::TimeTicks reference_time) {
  if (!flac_encoder_) {
    return;
  }
  flac_encoder_->Finish();
  auto data = flac_encoder_->GetAndClearAvailableEncodedData();
  if (data.empty()) {
    PushDataToAllListeners(nullptr, 0, reference_time);
    return;
  }
  PushDataToAllListeners(reinterpret_cast<uint8*>(&data[0]), data.size(),
                         reference_time);
}

}  // namespace encoders
}  // namespace media_capture
}  // namespace cobalt
