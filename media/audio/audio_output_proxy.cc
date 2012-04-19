// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_proxy.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_output_dispatcher.h"

namespace media {

AudioOutputProxy::AudioOutputProxy(AudioOutputDispatcher* dispatcher)
    : dispatcher_(dispatcher),
      state_(kCreated),
      volume_(1.0) {
}

AudioOutputProxy::~AudioOutputProxy() {
  DCHECK(CalledOnValidThread());
  DCHECK(state_ == kCreated || state_ == kClosed);
}

bool AudioOutputProxy::Open() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state_, kCreated);

  if (!dispatcher_->OpenStream()) {
    state_ = kError;
    return false;
  }

  state_ = kOpened;
  return true;
}

void AudioOutputProxy::Start(AudioSourceCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state_, kOpened);

  if (!dispatcher_->StartStream(callback, this)) {
    state_ = kError;
    callback->OnError(this, 0);
    return;
  }
  state_ = kPlaying;
}

void AudioOutputProxy::Stop() {
  DCHECK(CalledOnValidThread());
  if (state_ != kPlaying)
    return;

  dispatcher_->StopStream(this);
  state_ = kOpened;
}

void AudioOutputProxy::SetVolume(double volume) {
  DCHECK(CalledOnValidThread());
  volume_ = volume;
  dispatcher_->StreamVolumeSet(this, volume);
}

void AudioOutputProxy::GetVolume(double* volume) {
  DCHECK(CalledOnValidThread());
  *volume = volume_;
}

void AudioOutputProxy::Close() {
  DCHECK(CalledOnValidThread());
  DCHECK(state_ == kCreated || state_ == kError || state_ == kOpened);

  if (state_ != kCreated)
    dispatcher_->CloseStream(this);

  state_ = kClosed;

  // Delete the object now like is done in the Close() implementation of
  // physical stream objects.  If we delete the object via DeleteSoon, we
  // unnecessarily complicate the Shutdown procedure of the
  // dispatcher+audio manager.
  delete this;
}

}  // namespace media
