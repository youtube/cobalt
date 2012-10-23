// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fake_audio_output_stream.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "media/audio/audio_manager_base.h"

namespace media {

// static
AudioOutputStream* FakeAudioOutputStream::MakeFakeStream(
    AudioManagerBase* manager, const AudioParameters& params) {
  return new FakeAudioOutputStream(manager, params);
}

FakeAudioOutputStream::FakeAudioOutputStream(AudioManagerBase* manager,
                                             const AudioParameters& params)
    : audio_manager_(manager),
      callback_(NULL),
      audio_bus_(AudioBus::Create(params)),
      frames_per_millisecond_(
          params.sample_rate() / static_cast<float>(
              base::Time::kMillisecondsPerSecond)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_this_(this)) {
}

FakeAudioOutputStream::~FakeAudioOutputStream() {
  DCHECK(!callback_);
}

bool FakeAudioOutputStream::Open() {
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
  return true;
}

void FakeAudioOutputStream::Start(AudioSourceCallback* callback)  {
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
  callback_ = callback;
  audio_manager_->GetMessageLoop()->PostTask(FROM_HERE, base::Bind(
      &FakeAudioOutputStream::OnMoreDataTask, weak_this_.GetWeakPtr()));
}

void FakeAudioOutputStream::Stop() {
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
  callback_ = NULL;
}

void FakeAudioOutputStream::Close() {
  DCHECK(!callback_);
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
  weak_this_.InvalidateWeakPtrs();
  audio_manager_->ReleaseOutputStream(this);
}

void FakeAudioOutputStream::SetVolume(double volume) {};

void FakeAudioOutputStream::GetVolume(double* volume) {
  *volume = 0;
};

void FakeAudioOutputStream::OnMoreDataTask() {
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());

  audio_bus_->Zero();
  int frames_received = audio_bus_->frames();
  if (callback_) {
    frames_received = callback_->OnMoreData(
        audio_bus_.get(), AudioBuffersState());
  }

  // Calculate our sleep duration for simulated playback.  Sleep for at least
  // one millisecond so we don't spin the CPU.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(
          &FakeAudioOutputStream::OnMoreDataTask, weak_this_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(
          std::max(1.0f, frames_received / frames_per_millisecond_)));
}

}  // namespace media
