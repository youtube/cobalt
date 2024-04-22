// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/converting_audio_fifo.h"

#include "media/base/audio_bus.h"
#include "media/base/audio_converter.h"
#include "media/base/channel_mixer.h"

namespace media {

ConvertingAudioFifo::ConvertingAudioFifo(
    const AudioParameters& input_params,
    const AudioParameters& converted_params,
    ConvertingAudioFifo::OuputCallback output_callback)
    : output_callback_(std::move(output_callback)),
      input_params_(input_params),
      converted_params_(converted_params),
      converter_(std::make_unique<AudioConverter>(input_params,
                                                  converted_params,
                                                  /*disable_fifo=*/true)) {
  converter_->AddInput(this);
  converter_->PrimeWithSilence();
  min_input_frames_needed_ = converter_->GetMaxInputFramesRequested(
      converted_params_.frames_per_buffer());

  converted_audio_bus_ = AudioBus::Create(
      converted_params_.channels(), converted_params_.frames_per_buffer());
}

ConvertingAudioFifo::~ConvertingAudioFifo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  converter_->RemoveInput(this);
}

void ConvertingAudioFifo::Push(std::unique_ptr<AudioBus> input_bus) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_flushing_);

  total_frames_ += input_bus->frames();
  inputs_.emplace_back(EnsureExpectedChannelCount(std::move(input_bus)));

  // Immediately convert frames if we have enough.
  while (total_frames_ >= min_input_frames_needed_)
    Convert();
}

void ConvertingAudioFifo::Convert() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(total_frames_ >= min_input_frames_needed_ || is_flushing_)
      << "total_frames_=" << total_frames_
      << ",min_input_frames_needed_=" << min_input_frames_needed_
      << ",is_flushing_=" << is_flushing_;
  converter_->Convert(converted_audio_bus_.get());
  output_callback_.Run(converted_audio_bus_.get());
}

void ConvertingAudioFifo::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_flushing_ = true;

  // Convert all remaining frames.
  while (total_frames_)
    Convert();

  converter_->Reset();
  converter_->PrimeWithSilence();

  inputs_.clear();
  total_frames_ = 0;
  front_frame_index_ = 0;
  is_flushing_ = false;
}

double ConvertingAudioFifo::ProvideInput(AudioBus* audio_bus,
                                         uint32_t frames_delayed,
                                         const AudioGlitchInfo& glitch_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int frames_needed = audio_bus->frames();
  int frames_written = 0;

  // If we aren't flushing, this should only be called if we have enough
  // frames to completey satisfy the request.
  DCHECK(is_flushing_ || total_frames_ >= frames_needed)
      << "is_flushing_=" << is_flushing_ << ",total_frames_=" << total_frames_
      << ",frames_needed=" << frames_needed;

  // Write until we've fulfilled the request or run out of frames.
  while (frames_written < frames_needed && total_frames_) {
    const AudioBus* front = inputs_.front().get();

    int frames_in_front = front->frames() - front_frame_index_;
    int frames_to_write =
        std::min(frames_needed - frames_written, frames_in_front);

    front->CopyPartialFramesTo(front_frame_index_, frames_to_write,
                               frames_written, audio_bus);

    frames_written += frames_to_write;
    front_frame_index_ += frames_to_write;
    total_frames_ -= frames_to_write;

    if (front_frame_index_ == front->frames()) {
      // We exhausted all frames in the front buffer, remove it.
      inputs_.pop_front();
      front_frame_index_ = 0;
    }
  }

  // We should only run out of frames if we're flushing.
  if (frames_written != frames_needed) {
    DCHECK(is_flushing_);
    DCHECK(!total_frames_);
    DCHECK(!inputs_.size());
    audio_bus->ZeroFramesPartial(frames_written,
                                 frames_needed - frames_written);
  }

  return 1.0f;
}

std::unique_ptr<AudioBus> ConvertingAudioFifo::EnsureExpectedChannelCount(
    std::unique_ptr<AudioBus> audio_bus) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // No mixing required.
  if (audio_bus->channels() == input_params_.channels())
    return audio_bus;

  const int& incoming_channels = audio_bus->channels();
  if (!mixer_ || mixer_input_params_.channels() != incoming_channels) {
    // Both the format and the sample rate are unused for mixing, but we still
    // need to pass a value below.
    mixer_input_params_.Reset(input_params_.format(),
                              ChannelLayoutConfig::Guess(incoming_channels),
                              incoming_channels, input_params_.sample_rate());

    mixer_ = std::make_unique<ChannelMixer>(mixer_input_params_, input_params_);
  }

  auto mixed_bus =
      AudioBus::Create(input_params_.channels(), audio_bus->frames());

  mixer_->Transform(audio_bus.get(), mixed_bus.get());

  return mixed_bus;
}

}  // namespace media
