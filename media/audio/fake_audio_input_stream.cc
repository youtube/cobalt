// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fake_audio_input_stream.h"

#include "base/bind.h"
#include "media/audio/audio_manager_base.h"

using base::Time;
using base::TimeDelta;

namespace media {

AudioInputStream* FakeAudioInputStream::MakeFakeStream(
    AudioManagerBase* manager,
    const AudioParameters& params) {
  return new FakeAudioInputStream(manager, params);
}

FakeAudioInputStream::FakeAudioInputStream(AudioManagerBase* manager,
                                           const AudioParameters& params)
    : audio_manager_(manager),
      callback_(NULL),
      buffer_size_((params.channels() * params.bits_per_sample() *
                    params.frames_per_buffer()) / 8),
      thread_("FakeAudioRecordingThread"),
      callback_interval_(base::TimeDelta::FromMilliseconds(
          (params.frames_per_buffer() * 1000) / params.sample_rate())) {
}

FakeAudioInputStream::~FakeAudioInputStream() {}

bool FakeAudioInputStream::Open() {
  buffer_.reset(new uint8[buffer_size_]);
  memset(buffer_.get(), 0, buffer_size_);
  return true;
}

void FakeAudioInputStream::Start(AudioInputCallback* callback)  {
  DCHECK(!thread_.IsRunning());
  callback_ = callback;
  last_callback_time_ = Time::Now();
  thread_.Start();
  thread_.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeAudioInputStream::DoCallback, base::Unretained(this)),
      callback_interval_);
}

void FakeAudioInputStream::DoCallback() {
  DCHECK(callback_);
  callback_->OnData(this, buffer_.get(), buffer_size_, buffer_size_, 0.0);

  Time now = Time::Now();
  base::TimeDelta next_callback_time =
      last_callback_time_ + callback_interval_ * 2 - now;

  // If we are falling behind, try to catch up as much as we can in the next
  // callback.
  if (next_callback_time < base::TimeDelta())
    next_callback_time = base::TimeDelta();

  last_callback_time_ = now;
  thread_.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeAudioInputStream::DoCallback, base::Unretained(this)),
      next_callback_time);
}

void FakeAudioInputStream::Stop() {
  thread_.Stop();
}

void FakeAudioInputStream::Close() {
  if (callback_) {
    callback_->OnClose(this);
    callback_ = NULL;
  }
  audio_manager_->ReleaseInputStream(this);
}

double FakeAudioInputStream::GetMaxVolume() {
  return 0.0;
}

void FakeAudioInputStream::SetVolume(double volume) {}

double FakeAudioInputStream::GetVolume() {
  return 0.0;
}

void FakeAudioInputStream::SetAutomaticGainControl(bool enabled) {}

bool FakeAudioInputStream::GetAutomaticGainControl() {
  return false;
}

}  // namespace media
