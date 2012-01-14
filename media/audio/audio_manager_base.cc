// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager_base.h"

#include "base/bind.h"
#include "media/audio/audio_output_dispatcher.h"
#include "media/audio/audio_output_proxy.h"

static const int kStreamCloseDelaySeconds = 5;

const char AudioManagerBase::kDefaultDeviceName[] = "Default";
const char AudioManagerBase::kDefaultDeviceId[] = "default";

AudioManagerBase::AudioManagerBase()
    : audio_thread_("AudioThread"),
      num_active_input_streams_(0) {
}

AudioManagerBase::~AudioManagerBase() {
  Shutdown();
}

#ifndef NDEBUG
void AudioManagerBase::AddRef() const {
  const MessageLoop* loop = audio_thread_.message_loop();
  DCHECK(loop == NULL || loop != MessageLoop::current());
  AudioManager::AddRef();
}

void AudioManagerBase::Release() const {
  const MessageLoop* loop = audio_thread_.message_loop();
  DCHECK(loop == NULL || loop != MessageLoop::current());
  AudioManager::Release();
}
#endif

void AudioManagerBase::Init() {
  CHECK(audio_thread_.Start());
}

string16 AudioManagerBase::GetAudioInputDeviceModel() {
  return string16();
}

MessageLoop* AudioManagerBase::GetMessageLoop() {
  return audio_thread_.message_loop();
}

AudioOutputStream* AudioManagerBase::MakeAudioOutputStreamProxy(
    const AudioParameters& params) {
  DCHECK_EQ(MessageLoop::current(), GetMessageLoop());

  if (!initialized())
    return NULL;

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
  if (!initialized())
    return;

  DCHECK_NE(MessageLoop::current(), GetMessageLoop());

  // We must use base::Unretained since Shutdown might have been called from
  // the destructor and we can't alter the refcount of the object at that point.
  GetMessageLoop()->PostTask(FROM_HERE, base::Bind(
      &AudioManagerBase::ShutdownOnAudioThread,
      base::Unretained(this)));
  // Stop() will wait for any posted messages to be processed first.
  audio_thread_.Stop();
}

void AudioManagerBase::ShutdownOnAudioThread() {
  DCHECK_EQ(MessageLoop::current(), GetMessageLoop());

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
