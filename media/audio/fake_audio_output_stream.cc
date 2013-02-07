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
              base::Time::kMillisecondsPerSecond)) {
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
  on_more_data_cb_.Reset(base::Bind(
      &FakeAudioOutputStream::OnMoreDataTask, base::Unretained(this)));
  audio_manager_->GetMessageLoop()->PostTask(
      FROM_HERE, on_more_data_cb_.callback());
}

void FakeAudioOutputStream::Stop() {
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
  callback_ = NULL;
  on_more_data_cb_.Cancel();
}

void FakeAudioOutputStream::Close() {
  DCHECK(!callback_);
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
  audio_manager_->ReleaseOutputStream(this);
}

void FakeAudioOutputStream::SetVolume(double volume) {};

void FakeAudioOutputStream::GetVolume(double* volume) {
  *volume = 0;
};

void FakeAudioOutputStream::OnMoreDataTask() {
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
  DCHECK(callback_);

  audio_bus_->Zero();
  int frames_received = callback_->OnMoreData(
      audio_bus_.get(), AudioBuffersState());

  // Calculate our sleep duration for simulated playback.  Sleep for at least
  // one millisecond so we don't spin the CPU.
  audio_manager_->GetMessageLoop()->PostDelayedTask(
      FROM_HERE, on_more_data_cb_.callback(), base::TimeDelta::FromMilliseconds(
          std::max(1.0f, frames_received / frames_per_millisecond_)));
}

}  // namespace media
