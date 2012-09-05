// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fake_audio_output_stream.h"

#include "base/at_exit.h"
#include "base/logging.h"
#include "media/audio/audio_manager_base.h"

namespace media {

FakeAudioOutputStream* FakeAudioOutputStream::current_fake_stream_ = NULL;

// static
AudioOutputStream* FakeAudioOutputStream::MakeFakeStream(
    AudioManagerBase* manager,
    const AudioParameters& params) {
  FakeAudioOutputStream* new_stream = new FakeAudioOutputStream(manager,
                                                                params);
  DCHECK(current_fake_stream_ == NULL);
  current_fake_stream_ = new_stream;
  return new_stream;
}

bool FakeAudioOutputStream::Open() {
  return true;
}

// static
FakeAudioOutputStream* FakeAudioOutputStream::GetCurrentFakeStream() {
  return current_fake_stream_;
}

void FakeAudioOutputStream::Start(AudioSourceCallback* callback)  {
  callback_ = callback;
  audio_bus_->Zero();
  callback_->OnMoreData(audio_bus_.get(), AudioBuffersState(0, 0));
}

void FakeAudioOutputStream::Stop() {
  callback_ = NULL;
}

void FakeAudioOutputStream::SetVolume(double volume) {
  volume_ = volume;
}

void FakeAudioOutputStream::GetVolume(double* volume) {
  *volume = volume_;
}

void FakeAudioOutputStream::Close() {
  closed_ = true;
  audio_manager_->ReleaseOutputStream(this);
}

FakeAudioOutputStream::FakeAudioOutputStream(AudioManagerBase* manager,
                                             const AudioParameters& params)
    : audio_manager_(manager),
      volume_(0),
      callback_(NULL),
      closed_(false) {
  audio_bus_ = AudioBus::Create(params);
}

FakeAudioOutputStream::~FakeAudioOutputStream() {
  if (current_fake_stream_ == this)
    current_fake_stream_ = NULL;
}

}  // namespace media
