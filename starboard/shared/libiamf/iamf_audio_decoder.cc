// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/libiamf/iamf_audio_decoder.h"

#include "starboard/common/string.h"
#include "third_party/libiamf/source/code/include/IAMF_defines.h"

namespace starboard {
namespace shared {
namespace libiamf {

namespace {
using shared::starboard::player::DecodedAudio;

constexpr int kForceBinauralAudio = false;

std::string ErrorCodeToString(int code) {
  switch (code) {
    case IAMF_OK:
      return "IAMF_OK";
    case IAMF_ERR_BAD_ARG:
      return "IAMF_ERR_BAD_ARG";
    case IAMF_ERR_BUFFER_TOO_SMALL:
      return "IAMF_ERR_BUFFER_TOO_SMALL";
    case IAMF_ERR_INTERNAL:
      return "IAMF_ERR_INTERNAL";
    case IAMF_ERR_INVALID_PACKET:
      return "IAMF_ERR_INVALID_PACKET";
    case IAMF_ERR_INVALID_STATE:
      return "IAMF_ERR_INVALID_STATE";
    case IAMF_ERR_UNIMPLEMENTED:
      return "IAMF_ERR_UNIMPLEMENTED";
    case IAMF_ERR_ALLOC_FAIL:
      return "IAMF_ERR_ALLOC_FAIL";
    default:
      return "Unknown IAMF error code " + std::to_string(code);
  }
}
}  // namespace

IamfAudioDecoder::IamfAudioDecoder(const AudioStreamInfo& audio_stream_info)
    : audio_stream_info_(audio_stream_info),
      channels_(audio_stream_info.number_of_channels) {
  decoder_ = IAMF_decoder_open();
  if (!decoder_) {
    SB_LOG(ERROR) << "Error creating libiamf decoder";
    ReportError("Error creating libiamf decoder");
  }
}

IamfAudioDecoder::~IamfAudioDecoder() {
  TeardownDecoder();
}

bool IamfAudioDecoder::is_valid() const {
  return decoder_ != NULL;
}

void IamfAudioDecoder::Initialize(const OutputCB& output_cb,
                                  const ErrorCB& error_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb);
  SB_DCHECK(!output_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  output_cb_ = output_cb;
  error_cb_ = error_cb;
}

void IamfAudioDecoder::Decode(const InputBuffers& input_buffers,
                              const ConsumedCB& consumed_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(!input_buffers.empty());
  SB_DCHECK(pending_audio_buffers_.empty());
  SB_DCHECK(output_cb_);

  if (stream_ended_) {
    SB_LOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }

  for (const auto& input_buffer : input_buffers) {
    if (!DecodeInternal(input_buffer)) {
      return;
    }
  }
  Schedule(consumed_cb);
}

bool IamfAudioDecoder::DecodeInternal(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffer);
  SB_DCHECK(output_cb_);
  SB_DCHECK(!stream_ended_ || !pending_audio_buffers_.empty());
  SB_DCHECK(is_valid());

  if (input_buffer->size() == 0) {
    SB_LOG(ERROR) << "Empty input buffer written to IamfAudioDecoder";
    ReportError("Empty input buffer written to IamfAudioDecoder");
    return false;
  }

  IamfBufferInfo info;
  if (!ParseInputBuffer(input_buffer, &info, kForceBinauralAudio,
                        channels_ > 2)) {
    SB_LOG(ERROR) << "Failed to parse IA Descriptors";
    ReportError("Failed to parse IA Descriptors");
    return false;
  }
  if (!decoder_is_configured_) {
    std::string error_message;
    if (!ConfigureDecoder(&info, input_buffer->timestamp(), &error_message)) {
      SB_LOG(ERROR) << "Failed to configure IAMF decoder";
      ReportError("Failed to configure IAMF decoder. " + error_message);
      return false;
    }
  }

  scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
      channels_, GetSampleType(), kSbMediaAudioFrameStorageTypeInterleaved,
      input_buffer->timestamp(),
      channels_ * info.num_samples *
          starboard::media::GetBytesPerSample(GetSampleType()));
  int samples_decoded =
      IAMF_decoder_decode(decoder_, info.data.data(), info.data_size, nullptr,
                          reinterpret_cast<void*>(decoded_audio->data()));
  if (samples_decoded < 1) {
    SB_LOG(ERROR) << "Failed to decode IAMF sample, error "
                  << ErrorCodeToString(samples_decoded);
    ReportError("Failed to decode IAMF sample, error " +
                ErrorCodeToString(samples_decoded));
    return false;
  }

  SB_DCHECK(samples_decoded <= info.num_samples)
      << "Samples decoded (" << samples_decoded
      << ") is greater than the number of samples indicated by the stream ("
      << info.num_samples << ")";

  if (samples_per_second_ == 0) {
    samples_per_second_ = info.sample_rate;
  }

  // TODO: Enable partial audio once float32 pcm output is available.
  const auto& sample_info = input_buffer->audio_sample_info();
  decoded_audio->AdjustForDiscardedDurations(
      samples_per_second_, sample_info.discarded_duration_from_front,
      sample_info.discarded_duration_from_back);

  decoded_audios_.push(decoded_audio);

  output_cb_();

  return true;
}

void IamfAudioDecoder::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);

  stream_ended_ = true;
  if (!pending_audio_buffers_.empty()) {
    return;
  }

  // Put EOS into the queue.
  decoded_audios_.push(new DecodedAudio);

  Schedule(output_cb_);
}

bool IamfAudioDecoder::ConfigureDecoder(IamfBufferInfo* info,
                                        int64_t timestamp,
                                        std::string* error_message) {
  SB_DCHECK(is_valid());
  SB_DCHECK(!decoder_is_configured_);

  // TODO: libiamf has an issue outputting 32 bit float samples, set to 16 bit
  // for now.
  int error = IAMF_decoder_set_bit_depth(decoder_, 16);
  if (error != IAMF_OK) {
    *error_message = "IAMF_decoder_set_bit_depth() fails with error " +
                     ErrorCodeToString(error);
    return false;
  }

  error = IAMF_decoder_set_sampling_rate(decoder_, info->sample_rate);
  if (error != IAMF_OK) {
    *error_message = "IAMF_decoder_set_sampling_rate() fails with error " +
                     ErrorCodeToString(error);
    return false;
  }

  if (kForceBinauralAudio) {
    SB_LOG(INFO) << "Configuring IamfAudioDecoder for binaural output";
    error = IAMF_decoder_output_layout_set_binaural(decoder_);
    if (error != IAMF_OK) {
      *error_message =
          "IAMF_decoder_output_layout_set_binaural() fails with error " +
          ErrorCodeToString(error);
      return false;
    }
  } else {
    IAMF_SoundSystem sound_system = SOUND_SYSTEM_A;
    if (channels_ == 6) {
      SB_LOG(INFO) << "Configuring IamfAudioDecoder for 5.1 output";
      sound_system = SOUND_SYSTEM_B;
    } else if (channels_ == 8) {
      SB_LOG(INFO) << "Configuring IamfAudioDecoder for 7.1 output";
      sound_system = SOUND_SYSTEM_I;
    } else {
      SB_LOG(INFO) << "Configuring IamfAudioDecoder for stereo output";
    }

    error = IAMF_decoder_output_layout_set_sound_system(decoder_, sound_system);
    if (error != IAMF_OK) {
      *error_message =
          "IAMF_decoder_output_layout_set_sound_system() fails with error " +
          ErrorCodeToString(error);
      return false;
    }
  }

  // Time base is set to 90000, as it is in the iamfplayer example
  // https://github.com/AOMediaCodec/libiamf/blob/v1.0.0-errata/code/test/tools/iamfplayer/player/iamfplayer.c#L450
  error = IAMF_decoder_set_pts(decoder_, timestamp, 90000);
  if (error != IAMF_OK) {
    *error_message =
        "IAMF_decoder_set_pts() fails with error " + ErrorCodeToString(error);
    return false;
  }

  error = IAMF_decoder_set_mix_presentation_id(
      decoder_, info->mix_presentation_id.value());
  if (error != IAMF_OK) {
    *error_message =
        "IAMF_decoder_set_mix_presentation_id() fails with error " +
        ErrorCodeToString(error);
    return false;
  }

  error = IAMF_decoder_peak_limiter_enable(decoder_, 0);
  if (error != IAMF_OK) {
    *error_message = "IAMF_decoder_peak_limiter_enable() fails with error " +
                     ErrorCodeToString(error);
    return false;
  }

  error = IAMF_decoder_set_normalization_loudness(decoder_, .0f);
  if (error != IAMF_OK) {
    *error_message =
        "IAMF_decoder_set_normalization_loudness() fails with error " +
        ErrorCodeToString(error);
    return false;
  }

  error = IAMF_decoder_configure(decoder_, info->config_obus.data(),
                                 info->config_obus_size, nullptr);
  if (error != IAMF_OK) {
    *error_message =
        "IAMF_decoder_configure() fails with error " + ErrorCodeToString(error);
    return false;
  }

  decoder_is_configured_ = true;

  return true;
}

void IamfAudioDecoder::TeardownDecoder() {
  if (is_valid()) {
    IAMF_decoder_close(decoder_);
    decoder_ = nullptr;
  }
}

scoped_refptr<IamfAudioDecoder::DecodedAudio> IamfAudioDecoder::Read(
    int* samples_per_second) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);
  SB_DCHECK(!decoded_audios_.empty());
  SB_DCHECK(samples_per_second_ > 0);

  scoped_refptr<DecodedAudio> result;
  if (!decoded_audios_.empty()) {
    result = decoded_audios_.front();
    decoded_audios_.pop();
  }
  *samples_per_second = samples_per_second_;
  return result;
}

void IamfAudioDecoder::Reset() {
  SB_DCHECK(BelongsToCurrentThread());

  if (is_valid()) {
    TeardownDecoder();
    decoder_ = IAMF_decoder_open();
  }

  decoder_is_configured_ = false;

  stream_ended_ = false;
  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }
  pending_audio_buffers_.clear();
  consumed_cb_ = nullptr;

  CancelPendingJobs();
}

SbMediaAudioSampleType IamfAudioDecoder::GetSampleType() const {
  SB_DCHECK(BelongsToCurrentThread());
#if SB_API_VERSION <= 15 && SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
  return kSbMediaAudioSampleTypeInt16;
#endif  // SB_API_VERSION <= 15 && SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
  return kSbMediaAudioSampleTypeInt16Deprecated;
}

void IamfAudioDecoder::ReportError(const std::string& message) const {
  SB_DCHECK(error_cb_);
  SB_LOG(ERROR) << "IamfAudioDecoder error: " << message;
  error_cb_(kSbPlayerErrorDecode, message);
}

}  // namespace libiamf
}  // namespace shared
}  // namespace starboard
