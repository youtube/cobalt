/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/speech/audio_encoder_flac.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"

namespace cobalt {
namespace speech {

namespace {
const char kContentTypeFLAC[] = "audio/x-flac; rate=";
const int kFLACCompressionLevel = 0;  // 0 for speed
const int kBitsPerSample = 16;
const float kMaxInt16AsFloat32 = 32767.0f;
}  // namespace

AudioEncoderFlac::AudioEncoderFlac(int sample_rate)
    : encoder_(FLAC__stream_encoder_new()) {
  DCHECK(encoder_);

  // Set the number of channels to be encoded.
  FLAC__stream_encoder_set_channels(encoder_, 1);
  // Set the sample resolution of the input to be encoded.
  FLAC__stream_encoder_set_bits_per_sample(encoder_, kBitsPerSample);
  // Set the sample rate (in Hz) of the input to be encoded.
  FLAC__stream_encoder_set_sample_rate(encoder_,
                                       static_cast<uint32>(sample_rate));
  // Set the compression level. A higher level usually means more computation
  // but higher compression.
  FLAC__stream_encoder_set_compression_level(encoder_, kFLACCompressionLevel);

  // Initialize the encoder instance to encode native FLAC stream.
  FLAC__StreamEncoderInitStatus encoder_status =
      FLAC__stream_encoder_init_stream(encoder_, WriteCallback, NULL, NULL,
                                       NULL, this);
  DCHECK_EQ(encoder_status, FLAC__STREAM_ENCODER_INIT_STATUS_OK);
}

AudioEncoderFlac::~AudioEncoderFlac() {
  DCHECK(thread_checker_.CalledOnValidThread());
  FLAC__stream_encoder_delete(encoder_);
}

void AudioEncoderFlac::Encode(const AudioBus* audio_bus) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK_EQ(audio_bus->channels(), 1);
  const float* audio_data = audio_bus->channel(0);
  uint32 frames = static_cast<uint32>(audio_bus->frames());
  scoped_array<FLAC__int32> flac_samples(new FLAC__int32[frames]);
  for (uint32 i = 0; i < frames; ++i) {
    flac_samples[i] =
        static_cast<FLAC__int32>(audio_data[i] * kMaxInt16AsFloat32);
  }

  FLAC__int32* flac_samples_ptr = flac_samples.get();
  // Submit data for encoding.
  FLAC__bool success =
      FLAC__stream_encoder_process(encoder_, &flac_samples_ptr, frames);
  DCHECK(success);
}

void AudioEncoderFlac::Finish() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Finish the encoding. It causes the encoder to encode any data still in
  // its input pipe, and finally reset the encoder to the unintialized state.
  FLAC__stream_encoder_finish(encoder_);
}

std::string AudioEncoderFlac::GetMimeType() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return std::string(kContentTypeFLAC) +
         base::UintToString(FLAC__stream_encoder_get_sample_rate(encoder_));
}

std::string AudioEncoderFlac::GetAndClearAvailableEncodedData() {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::string result = encoded_data_;
  encoded_data_.clear();
  return result;
}

// A write callback which will be called anytime there is a raw encoded data to
// write. The call to FLAC__stream_encoder_init_stream() currently will also
// immediately call the write callback several times, once with the FLAC
// signature, and once for each encoded metadata block.
FLAC__StreamEncoderWriteStatus AudioEncoderFlac::WriteCallback(
    const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes,
    unsigned int samples, unsigned int current_frame, void* client_data) {
  UNREFERENCED_PARAMETER(encoder);
  UNREFERENCED_PARAMETER(samples);
  UNREFERENCED_PARAMETER(current_frame);

  AudioEncoderFlac* audio_encoder =
      reinterpret_cast<AudioEncoderFlac*>(client_data);
  DCHECK(audio_encoder);
  DCHECK(audio_encoder->thread_checker_.CalledOnValidThread());

  audio_encoder->encoded_data_.append(reinterpret_cast<const char*>(buffer),
                                      bytes);

  return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

}  // namespace speech
}  // namespace cobalt
