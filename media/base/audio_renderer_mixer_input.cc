// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_renderer_mixer_input.h"

#include "base/logging.h"
#include "media/base/audio_renderer_mixer.h"

namespace media {

AudioRendererMixerInput::AudioRendererMixerInput(
    const scoped_refptr<AudioRendererMixer>& mixer)
    : playing_(false),
      initialized_(false),
      volume_(1.0f),
      mixer_(mixer),
      callback_(NULL) {
}

AudioRendererMixerInput::~AudioRendererMixerInput() {
  if (!initialized_)
    return;

  // Clean up |audio_data_|.
  for (size_t i = 0; i < audio_data_.size(); ++i)
    delete [] audio_data_[i];
  audio_data_.clear();
}

void AudioRendererMixerInput::Initialize(
    const AudioParameters& params,
    AudioRendererSink::RenderCallback* callback) {
  DCHECK(!initialized_);

  // TODO(dalecurtis): If we switch to AVX/SSE optimization, we'll need to
  // allocate these on 32-byte boundaries and ensure they're sized % 32 bytes.
  audio_data_.reserve(params.channels());
  for (int i = 0; i < params.channels(); ++i)
    audio_data_.push_back(new float[params.frames_per_buffer()]);

  callback_ = callback;
  initialized_ = true;
}

void AudioRendererMixerInput::Start() {
  DCHECK(initialized_);
  mixer_->AddMixerInput(this);
}

void AudioRendererMixerInput::Stop() {
  DCHECK(initialized_);
  playing_ = false;
  mixer_->RemoveMixerInput(this);
}

void AudioRendererMixerInput::Play() {
  playing_ = true;
}

void AudioRendererMixerInput::Pause(bool /* flush */) {
  // We don't care about flush since Pause() simply indicates we should send
  // silence to AudioRendererMixer.
  playing_ = false;
}

bool AudioRendererMixerInput::SetVolume(double volume) {
  volume_ = volume;
  return true;
}

void AudioRendererMixerInput::GetVolume(double* volume) {
  *volume = volume_;
}

}  // namespace media
