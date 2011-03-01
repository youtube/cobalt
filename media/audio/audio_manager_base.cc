// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_output_dispatcher.h"
#include "media/audio/audio_output_proxy.h"

namespace {
const int kStreamCloseDelayMs = 5000;
}  // namespace

AudioManagerBase::AudioManagerBase()
    : audio_thread_("AudioThread"),
      initialized_(false) {
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
