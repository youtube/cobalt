// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_dispatcher.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "media/audio/audio_io.h"

AudioOutputDispatcher::AudioOutputDispatcher(
    AudioManager* audio_manager, const AudioParameters& params,
    int close_delay_ms)
    : audio_manager_(audio_manager),
      message_loop_(audio_manager->GetMessageLoop()),
      params_(params),
      pause_delay_milliseconds_(2 * params.samples_per_packet *
          base::Time::kMillisecondsPerSecond / params.sample_rate),
      paused_proxies_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(close_timer_(FROM_HERE,
          base::TimeDelta::FromMilliseconds(close_delay_ms),
          this, &AudioOutputDispatcher::ClosePendingStreams)) {
}

AudioOutputDispatcher::~AudioOutputDispatcher() {
}

bool AudioOutputDispatcher::StreamOpened() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  paused_proxies_++;

  // Ensure that there is at least one open stream.
  if (idle_streams_.empty() && !CreateAndOpenStream()) {
    return false;
  }

  close_timer_.Reset();

  return true;
}

AudioOutputStream* AudioOutputDispatcher::StreamStarted() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  if (idle_streams_.empty() && !CreateAndOpenStream()) {
    return NULL;
  }

  AudioOutputStream* stream = idle_streams_.back();
  idle_streams_.pop_back();

  DCHECK_GT(paused_proxies_, 0u);
  paused_proxies_--;

  close_timer_.Reset();

  // Schedule task to allocate streams for other proxies if we need to.
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioOutputDispatcher::OpenTask, this));

  return stream;
}

void AudioOutputDispatcher::StreamStopped(AudioOutputStream* stream) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  paused_proxies_++;

  pausing_streams_.push_front(stream);

  // Don't recycle stream until two buffers worth of time has elapsed.
  message_loop_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AudioOutputDispatcher::StopStreamTask, this),
      pause_delay_milliseconds_);
}

void AudioOutputDispatcher::StopStreamTask() {
  if (pausing_streams_.empty())
    return;
  AudioOutputStream* stream = pausing_streams_.back();
  pausing_streams_.pop_back();
  idle_streams_.push_back(stream);
  close_timer_.Reset();
}

void AudioOutputDispatcher::StreamClosed() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  while (!pausing_streams_.empty()) {
    idle_streams_.push_back(pausing_streams_.back());
    pausing_streams_.pop_back();
  }

  DCHECK_GT(paused_proxies_, 0u);
  paused_proxies_--;

  while (idle_streams_.size() > paused_proxies_) {
    idle_streams_.back()->Close();
    idle_streams_.pop_back();
  }
}

MessageLoop* AudioOutputDispatcher::message_loop() {
  return message_loop_;
}

bool AudioOutputDispatcher::CreateAndOpenStream() {
  AudioOutputStream* stream =
      audio_manager_->MakeAudioOutputStream(params_);
  if (!stream) {
    return false;
  }
  if (!stream->Open()) {
    stream->Close();
    return false;
  }
  idle_streams_.push_back(stream);
  return true;
}

void AudioOutputDispatcher::OpenTask() {
  // Make sure that we have at least one stream allocated if there
  // are paused streams.
  if (paused_proxies_ > 0 && idle_streams_.empty() &&
      pausing_streams_.empty()) {
    CreateAndOpenStream();
  }

  close_timer_.Reset();
}

// This method is called by |close_timer_|.
void AudioOutputDispatcher::ClosePendingStreams() {
  while (!idle_streams_.empty()) {
    idle_streams_.back()->Close();
    idle_streams_.pop_back();
  }
}
