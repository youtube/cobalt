// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_output_dispatcher.h"
#include "media/audio/audio_output_proxy.h"

static const int kStreamCloseDelayMs = 5000;

const char AudioManagerBase::kDefaultDeviceName[] = "Default";
const char AudioManagerBase::kDefaultDeviceId[] = "default";

AudioManagerBase::AudioManagerBase()
    : audio_thread_("AudioThread"),
      initialized_(false),
      num_active_input_streams_(0) {
}

AudioManagerBase::~AudioManagerBase() {
  DCHECK(!audio_thread_.IsRunning());
}

void AudioManagerBase::Init() {
  initialized_ = audio_thread_.Start();
}

void AudioManagerBase::Cleanup() {
  if (initialized_) {
    initialized_ = false;
    audio_thread_.Stop();
  }
}

string16 AudioManagerBase::GetAudioInputDeviceModel() {
  return string16();
}

MessageLoop* AudioManagerBase::GetMessageLoop() {
  DCHECK(initialized_);
  return audio_thread_.message_loop();
}

AudioOutputStream* AudioManagerBase::MakeAudioOutputStreamProxy(
    const AudioParameters& params) {
  if (!initialized_)
    return NULL;

  scoped_refptr<AudioOutputDispatcher>& dispatcher =
      output_dispatchers_[params];
  if (!dispatcher)
    dispatcher = new AudioOutputDispatcher(this, params, kStreamCloseDelayMs);

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
  base::AutoLock auto_lock(active_input_streams_lock_);
  ++num_active_input_streams_;
}

void AudioManagerBase::DecreaseActiveInputStreamCount() {
  base::AutoLock auto_lock(active_input_streams_lock_);
  DCHECK_GT(num_active_input_streams_, 0);
  --num_active_input_streams_;
}

bool AudioManagerBase::IsRecordingInProcess() {
  base::AutoLock auto_lock(active_input_streams_lock_);
  return num_active_input_streams_ > 0;
}
