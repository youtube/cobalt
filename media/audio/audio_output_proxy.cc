// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_proxy.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_output_dispatcher.h"

AudioOutputProxy::AudioOutputProxy(AudioOutputDispatcher* dispatcher)
    : dispatcher_(dispatcher),
      state_(kCreated),
      physical_stream_(NULL),
      volume_(1.0) {
}

AudioOutputProxy::~AudioOutputProxy() {
  DCHECK(CalledOnValidThread());
  DCHECK(state_ == kCreated || state_ == kClosed);
  DCHECK(!physical_stream_);
}

bool AudioOutputProxy::Open() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state_, kCreated);

  if (!dispatcher_->StreamOpened()) {
    state_ = kError;
    return false;
  }

  state_ = kOpened;
  return true;
}

void AudioOutputProxy::Start(AudioSourceCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(physical_stream_ == NULL);
  DCHECK_EQ(state_, kOpened);

  physical_stream_= dispatcher_->StreamStarted();
  if (!physical_stream_) {
    state_ = kError;
    callback->OnError(this, 0);
    return;
  }

  physical_stream_->SetVolume(volume_);
  physical_stream_->Start(callback);
  state_ = kPlaying;
}

void AudioOutputProxy::Stop() {
  DCHECK(CalledOnValidThread());
  if (state_ != kPlaying)
    return;

  DCHECK(physical_stream_);
  physical_stream_->Stop();
  dispatcher_->StreamStopped(physical_stream_);
  physical_stream_ = NULL;
  state_ = kOpened;
}

void AudioOutputProxy::SetVolume(double volume) {
  DCHECK(CalledOnValidThread());
  volume_ = volume;
  if (physical_stream_) {
    physical_stream_->SetVolume(volume);
  }
}

void AudioOutputProxy::GetVolume(double* volume) {
  DCHECK(CalledOnValidThread());
  *volume = volume_;
}

void AudioOutputProxy::Close() {
  DCHECK(CalledOnValidThread());
  DCHECK(state_ == kCreated || state_ == kError || state_ == kOpened);
  DCHECK(!physical_stream_);

  if (state_ != kCreated)
    dispatcher_->StreamClosed();

  state_ = kClosed;

  // Delete the object now like is done in the Close() implementation of
  // physical stream objects.  If we delete the object via DeleteSoon, we
  // unnecessarily complicate the Shutdown procedure of the
  // dispatcher+audio manager.
  delete this;
}
