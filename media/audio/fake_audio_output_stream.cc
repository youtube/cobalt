// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "media/audio/fake_audio_output_stream.h"

bool FakeAudioOutputStream::has_created_fake_stream_ = false;
FakeAudioOutputStream* FakeAudioOutputStream::last_fake_stream_ = NULL;

// static
AudioOutputStream* FakeAudioOutputStream::MakeFakeStream() {
  if (!has_created_fake_stream_)
    base::AtExitManager::RegisterCallback(&DestroyLastFakeStream, NULL);
  has_created_fake_stream_ = true;

  return new FakeAudioOutputStream();
}

// static
FakeAudioOutputStream* FakeAudioOutputStream::GetLastFakeStream() {
  return last_fake_stream_;
}

bool FakeAudioOutputStream::Open(size_t packet_size) {
  if (packet_size < sizeof(int16))
    return false;
  packet_size_ = packet_size;
  buffer_.reset(new char[packet_size_]);
  return true;
}

void FakeAudioOutputStream::Start(AudioSourceCallback* callback)  {
  callback_ = callback;
  memset(buffer_.get(), 0, packet_size_);
  callback_->OnMoreData(this, buffer_.get(), packet_size_, 0);
}

void FakeAudioOutputStream::Stop() {
}

void FakeAudioOutputStream::SetVolume(double volume) {
  volume_ = volume;
}

void FakeAudioOutputStream::GetVolume(double* volume) {
  *volume = volume_;
}

void FakeAudioOutputStream::Close() {
  callback_->OnClose(this);
  callback_ = NULL;

  if (last_fake_stream_)
    delete last_fake_stream_;
  last_fake_stream_ = this;
}

FakeAudioOutputStream::FakeAudioOutputStream()
    : volume_(0),
      callback_(NULL),
      packet_size_(0) {
}

// static
void FakeAudioOutputStream::DestroyLastFakeStream(void* param) {
  if (last_fake_stream_)
    delete last_fake_stream_;
}
