// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager_base.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "base/threading/thread.h"
#include "media/audio/audio_output_dispatcher.h"
#include "media/audio/audio_output_proxy.h"

static const int kStreamCloseDelaySeconds = 5;

const char AudioManagerBase::kDefaultDeviceName[] = "Default";
const char AudioManagerBase::kDefaultDeviceId[] = "default";

AudioManagerBase::AudioManagerBase()
    : num_active_input_streams_(0) {
}

AudioManagerBase::~AudioManagerBase() {
  Shutdown();
}

#ifndef NDEBUG
void AudioManagerBase::AddRef() const {
  {
    base::AutoLock lock(audio_thread_lock_);
    const MessageLoop* loop = audio_thread_.get() ?
        audio_thread_->message_loop() : NULL;
    DCHECK(loop == NULL || loop != MessageLoop::current());
  }
  AudioManager::AddRef();
}

void AudioManagerBase::Release() const {
  {
    base::AutoLock lock(audio_thread_lock_);
    const MessageLoop* loop = audio_thread_.get() ?
        audio_thread_->message_loop() : NULL;
    DCHECK(loop == NULL || loop != MessageLoop::current());
  }
  AudioManager::Release();
}
#endif

void AudioManagerBase::Init() {
  base::AutoLock lock(audio_thread_lock_);
  DCHECK(!audio_thread_.get());
  audio_thread_.reset(new base::Thread("AudioThread"));
  CHECK(audio_thread_->Start());
}

string16 AudioManagerBase::GetAudioInputDeviceModel() {
  return string16();
}

scoped_refptr<base::MessageLoopProxy> AudioManagerBase::GetMessageLoop() {
  base::AutoLock lock(audio_thread_lock_);
  return audio_thread_.get() ? audio_thread_->message_loop_proxy() : NULL;
}

AudioOutputStream* AudioManagerBase::MakeAudioOutputStreamProxy(
    const AudioParameters& params) {
  DCHECK(GetMessageLoop()->BelongsToCurrentThread());

  scoped_refptr<AudioOutputDispatcher>& dispatcher =
      output_dispatchers_[params];
  if (!dispatcher)
    dispatcher = new AudioOutputDispatcher(
        this, params, base::TimeDelta::FromSeconds(kStreamCloseDelaySeconds));
  return new AudioOutputProxy(dispatcher);
}

bool AudioManagerBase::CanShowAudioInputSettings() {
  return false;
}

void AudioManagerBase::ShowAudioInputSettings() {
}

void AudioManagerBase::GetAudioInputDeviceNames(
    media::AudioDeviceNames* device_names) {
}

void AudioManagerBase::IncreaseActiveInputStreamCount() {
  base::AtomicRefCountInc(&num_active_input_streams_);
}

void AudioManagerBase::DecreaseActiveInputStreamCount() {
  DCHECK(IsRecordingInProcess());
  base::AtomicRefCountDec(&num_active_input_streams_);
}

bool AudioManagerBase::IsRecordingInProcess() {
  return !base::AtomicRefCountIsZero(&num_active_input_streams_);
}

void AudioManagerBase::Shutdown() {
  // To avoid running into deadlocks while we stop the thread, shut it down
  // via a local variable while not holding the audio thread lock.
  scoped_ptr<base::Thread> audio_thread;
  {
    base::AutoLock lock(audio_thread_lock_);
    audio_thread_.swap(audio_thread);
  }

  if (!audio_thread.get())
      return;

  CHECK_NE(MessageLoop::current(), audio_thread->message_loop());

  // We must use base::Unretained since Shutdown might have been called from
  // the destructor and we can't alter the refcount of the object at that point.
  audio_thread->message_loop()->PostTask(FROM_HERE, base::Bind(
      &AudioManagerBase::ShutdownOnAudioThread,
      base::Unretained(this)));

  // Stop() will wait for any posted messages to be processed first.
  audio_thread->Stop();
}

void AudioManagerBase::ShutdownOnAudioThread() {
  // This should always be running on the audio thread, but since we've cleared
  // the audio_thread_ member pointer when we get here, we can't verify exactly
  // what thread we're running on.  The method is not public though and only
  // called from one place, so we'll leave it at that.
  AudioOutputDispatchersMap::iterator it = output_dispatchers_.begin();
  for (; it != output_dispatchers_.end(); ++it) {
    scoped_refptr<AudioOutputDispatcher>& dispatcher = (*it).second;
    if (dispatcher) {
      dispatcher->Shutdown();
      // All AudioOutputProxies must have been freed before Shutdown is called.
      // If they still exist, things will go bad.  They have direct pointers to
      // both physical audio stream objects that belong to the dispatcher as
      // well as the message loop of the audio thread that will soon go away.
      // So, better crash now than later.
      CHECK(dispatcher->HasOneRef()) << "AudioOutputProxies are still alive";
      dispatcher = NULL;
    }
  }

  output_dispatchers_.clear();
}
