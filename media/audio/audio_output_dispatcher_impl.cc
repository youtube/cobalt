// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_dispatcher_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_output_proxy.h"
#include "media/audio/audio_util.h"

namespace media {

AudioOutputDispatcherImpl::AudioOutputDispatcherImpl(
    AudioManager* audio_manager,
    const AudioParameters& params,
    const base::TimeDelta& close_delay)
    : AudioOutputDispatcher(audio_manager, params),
      pause_delay_(base::TimeDelta::FromMilliseconds(
          2 * params.frames_per_buffer() *
          base::Time::kMillisecondsPerSecond / params.sample_rate())),
      paused_proxies_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_this_(this)),
      close_timer_(FROM_HERE,
                   close_delay,
                   weak_this_.GetWeakPtr(),
                   &AudioOutputDispatcherImpl::ClosePendingStreams) {
}

AudioOutputDispatcherImpl::~AudioOutputDispatcherImpl() {
  DCHECK(proxy_to_physical_map_.empty());
  DCHECK(idle_streams_.empty());
  DCHECK(pausing_streams_.empty());
}

bool AudioOutputDispatcherImpl::OpenStream() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  paused_proxies_++;

  // Ensure that there is at least one open stream.
  if (idle_streams_.empty() && !CreateAndOpenStream())
    return false;

  close_timer_.Reset();
  return true;
}

bool AudioOutputDispatcherImpl::StartStream(
    AudioOutputStream::AudioSourceCallback* callback,
    AudioOutputProxy* stream_proxy) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  if (idle_streams_.empty() && !CreateAndOpenStream())
    return false;

  AudioOutputStream* physical_stream = idle_streams_.back();
  DCHECK(physical_stream);
  idle_streams_.pop_back();

  DCHECK_GT(paused_proxies_, 0u);
  --paused_proxies_;

  close_timer_.Reset();

  // Schedule task to allocate streams for other proxies if we need to.
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioOutputDispatcherImpl::OpenTask, weak_this_.GetWeakPtr()));

  double volume = 0;
  stream_proxy->GetVolume(&volume);
  physical_stream->SetVolume(volume);
  physical_stream->Start(callback);
  proxy_to_physical_map_[stream_proxy] = physical_stream;
  return true;
}

void AudioOutputDispatcherImpl::StopStream(AudioOutputProxy* stream_proxy) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  AudioStreamMap::iterator it = proxy_to_physical_map_.find(stream_proxy);
  // StopStream() may have already been called.
  // TODO(dalecurtis): StopStream() shouldn't be called twice!  See:
  // http://crbug.com/149815 and http://crbug.com/150619
  if (it == proxy_to_physical_map_.end())
    return;
  AudioOutputStream* physical_stream = it->second;
  proxy_to_physical_map_.erase(it);

  physical_stream->Stop();

  ++paused_proxies_;

  pausing_streams_.push_front(physical_stream);

  // Don't recycle stream until two buffers worth of time has elapsed.
  message_loop_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AudioOutputDispatcherImpl::StopStreamTask,
                 weak_this_.GetWeakPtr()),
      pause_delay_);
}

void AudioOutputDispatcherImpl::StreamVolumeSet(AudioOutputProxy* stream_proxy,
                                                double volume) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  AudioStreamMap::iterator it = proxy_to_physical_map_.find(stream_proxy);
  if (it != proxy_to_physical_map_.end()) {
    AudioOutputStream* physical_stream = it->second;
    physical_stream->SetVolume(volume);
  }
}

void AudioOutputDispatcherImpl::StopStreamTask() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  if (pausing_streams_.empty())
    return;

  AudioOutputStream* stream = pausing_streams_.back();
  pausing_streams_.pop_back();
  idle_streams_.push_back(stream);
  close_timer_.Reset();
}

void AudioOutputDispatcherImpl::CloseStream(AudioOutputProxy* stream_proxy) {
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

void AudioOutputDispatcherImpl::Shutdown() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  // Cancel any pending tasks to close paused streams or create new ones.
  weak_this_.InvalidateWeakPtrs();

  // No AudioOutputProxy objects should hold a reference to us when we get
  // to this stage.
  DCHECK(HasOneRef()) << "Only the AudioManager should hold a reference";

  AudioOutputStreamList::iterator it = idle_streams_.begin();
  for (; it != idle_streams_.end(); ++it)
    (*it)->Close();
  idle_streams_.clear();

  it = pausing_streams_.begin();
  for (; it != pausing_streams_.end(); ++it)
    (*it)->Close();
  pausing_streams_.clear();
}

bool AudioOutputDispatcherImpl::CreateAndOpenStream() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  AudioOutputStream* stream = audio_manager_->MakeAudioOutputStream(params_);
  if (!stream)
    return false;

  if (!stream->Open()) {
    stream->Close();
    return false;
  }
  idle_streams_.push_back(stream);
  return true;
}

void AudioOutputDispatcherImpl::OpenTask() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  // Make sure that we have at least one stream allocated if there
  // are paused streams.
  if (paused_proxies_ > 0 && idle_streams_.empty() &&
      pausing_streams_.empty()) {
    CreateAndOpenStream();
  }

  close_timer_.Reset();
}

// This method is called by |close_timer_|.
void AudioOutputDispatcherImpl::ClosePendingStreams() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  while (!idle_streams_.empty()) {
    idle_streams_.back()->Close();
    idle_streams_.pop_back();
  }
}

}  // namespace media
