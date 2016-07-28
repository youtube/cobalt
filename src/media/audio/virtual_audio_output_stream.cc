// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/virtual_audio_output_stream.h"

#include "base/message_loop.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/virtual_audio_input_stream.h"

namespace media {

// static
VirtualAudioOutputStream* VirtualAudioOutputStream::MakeStream(
    AudioManagerBase* manager, const AudioParameters& params,
    base::MessageLoopProxy* message_loop, VirtualAudioInputStream* target) {
  return new VirtualAudioOutputStream(manager, params, message_loop, target);
}

VirtualAudioOutputStream::VirtualAudioOutputStream(
    AudioManagerBase* manager, const AudioParameters& params,
    base::MessageLoopProxy* message_loop, VirtualAudioInputStream* target)
    : audio_manager_(manager), message_loop_(message_loop), callback_(NULL),
      params_(params), target_input_stream_(target), volume_(1.0f),
      attached_(false) {
}

VirtualAudioOutputStream::~VirtualAudioOutputStream() {
  DCHECK(!callback_);
  DCHECK(!attached_);
}

bool VirtualAudioOutputStream::Open() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return true;
}

void VirtualAudioOutputStream::Start(AudioSourceCallback* callback)  {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!attached_);
  callback_ = callback;
  target_input_stream_->AddOutputStream(this, params_);
  attached_ = true;
}

void VirtualAudioOutputStream::Stop() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(attached_);
  callback_ = NULL;
  target_input_stream_->RemoveOutputStream(this, params_);
  attached_ = false;
}

void VirtualAudioOutputStream::Close() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  audio_manager_->ReleaseOutputStream(this);
}

void VirtualAudioOutputStream::SetVolume(double volume) {
  volume_ = volume;
}

void VirtualAudioOutputStream::GetVolume(double* volume) {
  *volume = volume_;
}

double VirtualAudioOutputStream::ProvideInput(AudioBus* audio_bus,
                                              base::TimeDelta buffer_delay) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(callback_);

  int frames = callback_->OnMoreData(audio_bus, AudioBuffersState());
  if (frames < audio_bus->frames())
    audio_bus->ZeroFramesPartial(frames, audio_bus->frames() - frames);

  return frames > 0 ? volume_ : 0;
}

}  // namespace media
