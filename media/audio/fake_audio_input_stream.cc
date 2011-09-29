// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fake_audio_input_stream.h"

#include "base/bind.h"

using base::Time;
using base::TimeDelta;

AudioInputStream* FakeAudioInputStream::MakeFakeStream(
    const AudioParameters& params) {
  return new FakeAudioInputStream(params);
}

FakeAudioInputStream::FakeAudioInputStream(const AudioParameters& params)
    : callback_(NULL),
      buffer_size_((params.channels * params.bits_per_sample *
                    params.samples_per_packet) / 8),
      thread_("FakeAudioRecordingThread"),
      callback_interval_ms_((params.samples_per_packet * 1000) /
                            params.sample_rate) {
  // This object is ref counted (so that it can be used with Thread, PostTask)
  // but the caller expects a plain pointer. So we take a reference here and
  // will Release() ourselves in Close().
  AddRef();
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
      base::Bind(&FakeAudioInputStream::DoCallback, this),
      callback_interval_ms_);
}

void FakeAudioInputStream::DoCallback() {
  DCHECK(callback_);
  callback_->OnData(this, buffer_.get(), buffer_size_);

  Time now = Time::Now();
  int64 next_callback_ms = (last_callback_time_ +
      TimeDelta::FromMilliseconds(callback_interval_ms_ * 2) -
      now).InMilliseconds();
  // If we are falling behind, try to catch up as much as we can in the next
  // callback.
  if (next_callback_ms < 0)
    next_callback_ms = 0;

  last_callback_time_ = now;
  thread_.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeAudioInputStream::DoCallback, this),
      next_callback_ms);
}

void FakeAudioInputStream::Stop() {
  thread_.Stop();
}

void FakeAudioInputStream::Close() {
  if (callback_) {
    callback_->OnClose(this);
    callback_ = NULL;
  }
  Release();  // Destoys this object.
}
