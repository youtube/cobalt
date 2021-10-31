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

#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/common/reset_and_return.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

using common::ResetAndReturn;

AdaptiveAudioDecoder::AdaptiveAudioDecoder(
    const SbMediaAudioSampleInfo& audio_sample_info,
    SbDrmSystem drm_system,
    const AudioDecoderCreator& audio_decoder_creator,
    const OutputFormatAdjustmentCallback& output_adjustment_callback)
    : initial_audio_sample_info_(audio_sample_info),
      drm_system_(drm_system),
      audio_decoder_creator_(audio_decoder_creator),
      output_adjustment_callback_(output_adjustment_callback),
      output_number_of_channels_(audio_sample_info.number_of_channels) {
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
  if (starboard::media::IsAudioSampleInfoSubstantiallyDifferent(
          input_audio_sample_info_, input_buffer->audio_sample_info())) {
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

scoped_refptr<DecodedAudio> AdaptiveAudioDecoder::Read(
    int* samples_per_second) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(!decoded_audios_.empty());

  scoped_refptr<DecodedAudio> ret = decoded_audios_.front();
  decoded_audios_.pop();

  SB_DCHECK(ret->is_end_of_stream() ||
            ret->sample_type() == output_sample_type_);
  SB_DCHECK(ret->is_end_of_stream() ||
            ret->storage_type() == output_storage_type_);
  SB_DCHECK(ret->is_end_of_stream() ||
            ret->channels() == output_number_of_channels_);

  SB_DCHECK(first_output_received_ || ret->is_end_of_stream());
  *samples_per_second = first_output_received_
                            ? output_samples_per_second_
                            : initial_audio_sample_info_.samples_per_second;
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
  SB_DCHECK(!resampler_);
  SB_DCHECK(!channel_mixer_);

  input_audio_sample_info_ = audio_sample_info;
  output_format_checked_ = false;
  audio_decoder_ =
      audio_decoder_creator_(input_audio_sample_info_, drm_system_);

  if (!audio_decoder_) {
    error_cb_(kSbPlayerErrorDecode, "Decoder adapter cannot create decoder.");
    return;
  }
  audio_decoder_->Initialize(
      std::bind(&AdaptiveAudioDecoder::OnDecoderOutput, this), error_cb_);
}

void AdaptiveAudioDecoder::TeardownAudioDecoder() {
  audio_decoder_.reset();
  resampler_.reset();
  channel_mixer_.reset();
}

void AdaptiveAudioDecoder::OnDecoderOutput() {
  if (!BelongsToCurrentThread()) {
    Schedule(std::bind(&AdaptiveAudioDecoder::OnDecoderOutput, this));
    return;
  }
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);

  int decoded_sample_rate;
  scoped_refptr<DecodedAudio> decoded_audio =
      audio_decoder_->Read(&decoded_sample_rate);
  if (!first_output_received_) {
    first_output_received_ = true;
    output_sample_type_ = decoded_audio->sample_type();
    output_storage_type_ = decoded_audio->storage_type();
    output_samples_per_second_ = decoded_sample_rate;
    if (output_adjustment_callback_) {
      output_adjustment_callback_(&output_sample_type_, &output_storage_type_,
                                  &output_samples_per_second_,
                                  &output_number_of_channels_);
    }
  }

  if (decoded_audio->is_end_of_stream()) {
    // Flush resampler.
    if (resampler_) {
      scoped_refptr<DecodedAudio> resampler_output =
          resampler_->WriteEndOfStream();
      if (resampler_output && resampler_output->size() > 0) {
        if (channel_mixer_) {
          resampler_output = channel_mixer_->Mix(resampler_output);
        }
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

  SB_DCHECK(input_audio_sample_info_.number_of_channels ==
            decoded_audio->channels());
  if (!output_format_checked_) {
    SB_DCHECK(!resampler_);
    SB_DCHECK(!channel_mixer_);
    output_format_checked_ = true;
    if (output_sample_type_ != decoded_audio->sample_type() ||
        output_storage_type_ != decoded_audio->storage_type() ||
        output_samples_per_second_ != decoded_sample_rate) {
      resampler_ = AudioResampler::Create(
          decoded_audio->sample_type(), decoded_audio->storage_type(),
          decoded_sample_rate, output_sample_type_, output_storage_type_,
          output_samples_per_second_,
          input_audio_sample_info_.number_of_channels);
    }
    if (input_audio_sample_info_.number_of_channels !=
        output_number_of_channels_) {
      channel_mixer_ = AudioChannelLayoutMixer::Create(
          output_sample_type_, output_storage_type_,
          output_number_of_channels_);
    }
  }
  if (resampler_) {
    decoded_audio = resampler_->Resample(decoded_audio);
  }
  if (decoded_audio && decoded_audio->size() > 0) {
    if (channel_mixer_) {
      decoded_audio = channel_mixer_->Mix(decoded_audio);
    }
    decoded_audios_.push(decoded_audio);
    Schedule(output_cb_);
  }
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
