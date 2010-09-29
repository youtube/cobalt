// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fake_audio_output_stream.h"

#include "base/at_exit.h"
#include "base/logging.h"

bool FakeAudioOutputStream::has_created_fake_stream_ = false;
FakeAudioOutputStream* FakeAudioOutputStream::last_fake_stream_ = NULL;

// static
AudioOutputStream* FakeAudioOutputStream::MakeFakeStream() {
  if (!has_created_fake_stream_)
    base::AtExitManager::RegisterCallback(&DestroyLastFakeStream, NULL);
  has_created_fake_stream_ = true;

  FakeAudioOutputStream* new_stream = new FakeAudioOutputStream();

  if (last_fake_stream_) {
    DCHECK(last_fake_stream_->closed_);
    delete last_fake_stream_;
  }
  last_fake_stream_ = new_stream;

  return new_stream;
}

// static
FakeAudioOutputStream* FakeAudioOutputStream::GetLastFakeStream() {
  return last_fake_stream_;
}

bool FakeAudioOutputStream::Open(uint32 packet_size) {
  if (packet_size < sizeof(int16))
    return false;
  packet_size_ = packet_size;
  buffer_.reset(new uint8[packet_size_]);
  return true;
}

void FakeAudioOutputStream::Start(AudioSourceCallback* callback)  {
  callback_ = callback;
  memset(buffer_.get(), 0, packet_size_);
  callback_->OnMoreData(this, buffer_.get(), packet_size_,
                        AudioBuffersState(0, 0));
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
  // Calls |callback_| only if it is valid. We don't have |callback_| if
  // we have not yet started.
  if (callback_) {
    callback_->OnClose(this);
    callback_ = NULL;
  }
  closed_ = true;
}

FakeAudioOutputStream::FakeAudioOutputStream()
    : volume_(0),
      callback_(NULL),
      packet_size_(0),
      closed_(false) {
}

// static
void FakeAudioOutputStream::DestroyLastFakeStream(void* param) {
  if (last_fake_stream_) {
    DCHECK(last_fake_stream_->closed_);
    delete last_fake_stream_;
  }
}
