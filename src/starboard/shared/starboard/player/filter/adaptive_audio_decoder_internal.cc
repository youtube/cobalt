// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/adaptive_audio_decoder_internal.h"

#include "starboard/common/reset_and_return.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

using common::ResetAndReturn;

#if SB_API_VERSION >= 11
bool IsResetDecoderNecessary(const SbMediaAudioSampleInfo& current_info,
                             const SbMediaAudioSampleInfo& new_info) {
  if (current_info.codec != new_info.codec) {
    return true;
  }
  if (current_info.samples_per_second != new_info.samples_per_second) {
    return true;
  }
  if (current_info.audio_specific_config_size !=
      new_info.audio_specific_config_size) {
    return true;
  }
  if (SbMemoryCompare(current_info.audio_specific_config,
                      new_info.audio_specific_config,
                      current_info.audio_specific_config_size) != 0) {
    return true;
  }
  return false;
}

AdaptiveAudioDecoder::AdaptiveAudioDecoder(
    const SbMediaAudioSampleInfo& audio_sample_info,
    SbDrmSystem drm_system,
    const AudioDecoderCreator& audio_decoder_creator,
    const OutputFormatAdjustmentCallback& output_adjustment_callback)
    : initilize_audio_sample_info_(audio_sample_info),
      drm_system_(drm_system),
      audio_decoder_creator_(audio_decoder_creator),
      output_adjustment_callback_(output_adjustment_callback) {
  SB_DCHECK(audio_sample_info.codec != kSbMediaAudioCodecNone);
}

AdaptiveAudioDecoder::~AdaptiveAudioDecoder() {
  Reset();
}

void AdaptiveAudioDecoder::Initialize(const OutputCB& output_cb,
                                      const ErrorCB& error_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb);
  SB_DCHECK(!output_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);
  SB_DCHECK(!audio_decoder_);
  SB_DCHECK(!stream_ended_);

  output_cb_ = output_cb;
  error_cb_ = error_cb;
}

void AdaptiveAudioDecoder::Decode(
    const scoped_refptr<InputBuffer>& input_buffer,
    const ConsumedCB& consumed_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(!stream_ended_);
  SB_DCHECK(output_cb_);
  SB_DCHECK(error_cb_);
  SB_DCHECK(!flushing_);
  SB_DCHECK(!pending_input_buffer_);
  SB_DCHECK(!pending_consumed_cb_);
  SB_DCHECK(input_buffer->sample_type() == kSbMediaTypeAudio);
  SB_DCHECK(input_buffer->audio_sample_info().codec != kSbMediaAudioCodecNone);

  if (!audio_decoder_) {
    InitializeAudioDecoder(input_buffer->audio_sample_info());
    if (audio_decoder_) {
      audio_decoder_->Decode(input_buffer, consumed_cb);
    }
    return;
  }
  if (IsResetDecoderNecessary(input_audio_sample_info_,
                              input_buffer->audio_sample_info())) {
    flushing_ = true;
    pending_input_buffer_ = input_buffer;
    pending_consumed_cb_ = consumed_cb;
    audio_decoder_->WriteEndOfStream();
  } else {
    audio_decoder_->Decode(input_buffer, consumed_cb);
  }
}

void AdaptiveAudioDecoder::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(!stream_ended_);
  SB_DCHECK(output_cb_);
  SB_DCHECK(error_cb_);
  SB_DCHECK(!pending_input_buffer_);
  SB_DCHECK(!pending_consumed_cb_);

  stream_ended_ = true;
  if (audio_decoder_) {
    audio_decoder_->WriteEndOfStream();
  } else {
    decoded_audios_.push(new DecodedAudio);
    Schedule(output_cb_);
  }
}

scoped_refptr<DecodedAudio> AdaptiveAudioDecoder::Read() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(!decoded_audios_.empty());

  scoped_refptr<DecodedAudio> ret = decoded_audios_.front();
  decoded_audios_.pop();
  return ret;
}

void AdaptiveAudioDecoder::Reset() {
  SB_DCHECK(BelongsToCurrentThread());

  if (audio_decoder_) {
    TeardownAudioDecoder();
  }
  CancelPendingJobs();
  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }
  pending_input_buffer_ = nullptr;
  pending_consumed_cb_ = nullptr;
  flushing_ = false;
  stream_ended_ = false;
  first_output_received_ = false;
}

void AdaptiveAudioDecoder::InitializeAudioDecoder(
    const SbMediaAudioSampleInfo& audio_sample_info) {
  SB_DCHECK(!audio_decoder_);
  SB_DCHECK(output_cb_);
  SB_DCHECK(error_cb_);

  input_audio_sample_info_ = audio_sample_info;
  output_format_checked_ = false;
  audio_decoder_ =
      audio_decoder_creator_(input_audio_sample_info_, drm_system_);

  if (!audio_decoder_) {
#if SB_HAS(PLAYER_ERROR_MESSAGE)
    error_cb_(kSbPlayerErrorDecode, "Decoder adapter cannot create decoder.");
#else   // SB_HAS(PLAYER_ERROR_MESSAGE)
    error_cb_();
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
    return;
  }
  audio_decoder_->Initialize(
      std::bind(&AdaptiveAudioDecoder::OnDecoderOutput, this), error_cb_);
}

void AdaptiveAudioDecoder::TeardownAudioDecoder() {
  audio_decoder_.reset();
  resampler_.reset();
}

void AdaptiveAudioDecoder::OnDecoderOutput() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);

  if (!first_output_received_) {
    first_output_received_ = true;
    output_sample_type_ = audio_decoder_->GetSampleType();
    output_storage_type_ = audio_decoder_->GetStorageType();
    output_samples_per_second_ = audio_decoder_->GetSamplesPerSecond();
    if (output_adjustment_callback_) {
      output_adjustment_callback_(&output_sample_type_, &output_storage_type_,
                                  &output_samples_per_second_);
    }
  }

  scoped_refptr<DecodedAudio> decoded_audio = audio_decoder_->Read();
  if (decoded_audio->is_end_of_stream()) {
    // Flush resampler.
    if (resampler_) {
      scoped_refptr<DecodedAudio> resampler_output =
          resampler_->WriteEndOfStream();
      if (resampler_output && resampler_output->size() > 0) {
        decoded_audios_.push(resampler_output);
        Schedule(output_cb_);
      }
    }
    if (flushing_) {
      SB_DCHECK(audio_decoder_);
      TeardownAudioDecoder();
      flushing_ = false;
      Decode(ResetAndReturn(&pending_input_buffer_),
             ResetAndReturn(&pending_consumed_cb_));
    } else {
      SB_DCHECK(stream_ended_);
      decoded_audios_.push(decoded_audio);
      Schedule(output_cb_);
    }
    return;
  }

  if (!output_format_checked_) {
    SB_DCHECK(!resampler_);
    output_format_checked_ = true;
    if (audio_decoder_->GetSampleType() != output_sample_type_ ||
        audio_decoder_->GetStorageType() != output_storage_type_ ||
        audio_decoder_->GetSamplesPerSecond() != output_samples_per_second_) {
      resampler_ = AudioResampler::Create(
          audio_decoder_->GetSampleType(), audio_decoder_->GetStorageType(),
          audio_decoder_->GetSamplesPerSecond(), output_sample_type_,
          output_storage_type_, output_samples_per_second_,
          initilize_audio_sample_info_.number_of_channels);
    }
  }
  if (resampler_) {
    decoded_audio = resampler_->Resample(decoded_audio);
  }
  if (decoded_audio && decoded_audio->size() > 0) {
    decoded_audios_.push(decoded_audio);
    Schedule(output_cb_);
  }
}
#endif  // SB_API_VERSION >= 11

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
