// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/blink/renderer/modules/mediarecorder/audio_track_flac_encoder.h"

#include <memory>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"

namespace {

const int kFLACCompressionLevel = 0;  // 0 for speed
const int kBitsPerSample = 32;

}  // anonymous namespace

namespace blink {

AudioTrackFlacEncoder::AudioTrackFlacEncoder(
    OnEncodedAudioCB on_encoded_audio_cb)
   : AudioTrackEncoder(std::move(on_encoded_audio_cb)), flac_encoder_{nullptr}{
      LOG(INFO) << "YO THOR - AUDIO TRACK FLAC ENCODER CTOR";
}

AudioTrackFlacEncoder::~AudioTrackFlacEncoder() {
  DestroyExistingFlacEncoder();
}

void AudioTrackFlacEncoder::OnSetFormat(
    const media::AudioParameters& input_params) {
  LOG(INFO) << "YO THOR _ ON SET FORMAT -:" << input_params.AsHumanReadableString();
  DVLOG(1) << __func__;
  if (input_params_.Equals(input_params))
    return;

  DestroyExistingFlacEncoder();

  if (!input_params.IsValid()) {
    DLOG(ERROR) << "Invalid params: " << input_params.AsHumanReadableString();
    LOG(ERROR) << "YO THOR _ Invalid params: " << input_params.AsHumanReadableString();
    return;
  }
  input_params_ = input_params;

  flac_encoder_ = FLAC__stream_encoder_new();
  DCHECK(flac_encoder_);

  // Set the number of channels to be encoded.
  FLAC__stream_encoder_set_channels(flac_encoder_, 1);
  // Set the sample resolution of the input to be encoded.
  FLAC__stream_encoder_set_bits_per_sample(flac_encoder_, kBitsPerSample);
  // Set the sample rate (in Hz) of the input to be encoded.
  FLAC__stream_encoder_set_sample_rate(flac_encoder_,
                                       static_cast<uint32_t>(input_params_.sample_rate()));
  // Set the compression level. A higher level usually means more computation
  // but higher compression.
  FLAC__stream_encoder_set_compression_level(flac_encoder_, kFLACCompressionLevel);
  
  // Initialize the encoder instance to encode native FLAC stream.
  FLAC__StreamEncoderInitStatus encoder_status =
      FLAC__stream_encoder_init_stream(flac_encoder_, WriteCallback, NULL, NULL,
                                       NULL, this);
  DCHECK_EQ(encoder_status, FLAC__STREAM_ENCODER_INIT_STATUS_OK);
}

void AudioTrackFlacEncoder::EncodeAudio(
    std::unique_ptr<media::AudioBus> input_bus,
    base::TimeTicks capture_time) {
  DVLOG(3) << __func__ << ", #frames " << input_bus->frames();
  DCHECK_EQ(input_bus->channels(), input_params_.channels());
  DCHECK(!capture_time.is_null());

  if (!is_initialized() || paused_)
    return;

  // DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK_EQ(input_bus->channels(), size_t(1));
  uint32_t frames = static_cast<uint32_t>(input_bus->frames());
  std::unique_ptr<FLAC__int32[]> flac_samples(new FLAC__int32[frames]);
  input_bus->ToInterleaved<media::SignedInt32SampleTypeTraits>(frames, flac_samples.get());

  LOG(INFO) << "YO THOR _ ENCODE AUDIO";
  FLAC__int32* flac_samples_ptr = flac_samples.get();
  // Submit data for encoding.
  FLAC__bool success =
      FLAC__stream_encoder_process(flac_encoder_, &flac_samples_ptr, frames);
  DCHECK(success);

  std::string encoded_data = std::move(encoded_data_);
  const base::TimeTicks capture_time_of_first_sample =
          capture_time - media::AudioTimestampHelper::FramesToTime(
                             input_bus->frames(), input_params_.sample_rate());
      on_encoded_audio_cb_.Run(input_params_, std::move(encoded_data),
                               capture_time_of_first_sample);

}

void AudioTrackFlacEncoder::DestroyExistingFlacEncoder() {
  // DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  FLAC__stream_encoder_delete(flac_encoder_);
}

// A write callback which will be called anytime there is a raw encoded data to
// write. The call to FLAC__stream_encoder_init_stream() currently will also
// immediately call the write callback several times, once with the FLAC
// signature, and once for each encoded metadata block.
FLAC__StreamEncoderWriteStatus AudioTrackFlacEncoder::WriteCallback(
    const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes,
    unsigned int samples, unsigned int current_frame, void* client_data) {
  AudioTrackFlacEncoder* audio_encoder =
      reinterpret_cast<AudioTrackFlacEncoder*>(client_data);
  DCHECK(audio_encoder);
  LOG(INFO) << "YO THOR _ WRITE CALL BACK!";
  // DCHECK_CALLED_ON_VALID_THREAD(audio_encoder->thread_checker_);

  audio_encoder->encoded_data_.append(reinterpret_cast<const char*>(buffer),
                                      bytes);

  return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

}  // namespace blink
