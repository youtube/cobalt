// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_renderer_mixer_input.h"

#include "base/logging.h"
#include "media/base/audio_renderer_mixer.h"

namespace media {

AudioRendererMixerInput::AudioRendererMixerInput(
    const GetMixerCB& get_mixer_cb, const RemoveMixerCB& remove_mixer_cb)
    : playing_(false),
      initialized_(false),
      volume_(1.0f),
      get_mixer_cb_(get_mixer_cb),
      remove_mixer_cb_(remove_mixer_cb),
      mixer_(NULL),
      callback_(NULL),
      current_audio_delay_milliseconds_(0) {
}

AudioRendererMixerInput::~AudioRendererMixerInput() {
  // Mixer is no longer safe to use after |remove_mixer_cb_| has been called.
  if (initialized_)
    remove_mixer_cb_.Run(params_);
}

void AudioRendererMixerInput::Initialize(
    const AudioParameters& params,
    AudioRendererSink::RenderCallback* callback) {
  DCHECK(!initialized_);
  params_ = params;
  mixer_ = get_mixer_cb_.Run(params_);
  callback_ = callback;
  initialized_ = true;
}

void AudioRendererMixerInput::Start() {
  DCHECK(initialized_);
  DCHECK(!playing_);
}

void AudioRendererMixerInput::Stop() {
  // Stop() may be called at any time, if Pause() hasn't been called we need to
  // remove our mixer input before shutdown.
  if (!playing_)
    return;

  mixer_->RemoveMixerInput(this);
  playing_ = false;
}

void AudioRendererMixerInput::Play() {
  DCHECK(initialized_);

  if (playing_)
    return;

  mixer_->AddMixerInput(this);
  playing_ = true;
}

void AudioRendererMixerInput::Pause(bool /* flush */) {
  DCHECK(initialized_);

  if (!playing_)
    return;

  mixer_->RemoveMixerInput(this);
  playing_ = false;
}

bool AudioRendererMixerInput::SetVolume(double volume) {
  volume_ = volume;
  return true;
}

double AudioRendererMixerInput::ProvideInput(AudioBus* audio_bus,
                                             base::TimeDelta buffer_delay) {
  int frames_filled = callback_->Render(
      audio_bus,
      current_audio_delay_milliseconds_ + buffer_delay.InMilliseconds());

  // AudioConverter expects unfilled frames to be zeroed.
  if (frames_filled < audio_bus->frames()) {
    audio_bus->ZeroFramesPartial(
        frames_filled, audio_bus->frames() - frames_filled);
  }

  return frames_filled > 0 ? volume_ : 0;
}

void AudioRendererMixerInput::OnRenderError() {
  callback_->OnRenderError();
}

}  // namespace media
