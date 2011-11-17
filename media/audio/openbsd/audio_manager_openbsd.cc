// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/openbsd/audio_manager_openbsd.h"

// Implementation of AudioManager.
bool AudioManagerOpenBSD::HasAudioOutputDevices() {
  NOTIMPLEMENTED();
  return false;
}

bool AudioManagerOpenBSD::HasAudioInputDevices() {
  NOTIMPLEMENTED();
  return false;
}

AudioOutputStream* AudioManagerOpenBSD::MakeAudioOutputStream(
    const AudioParameters& params) {
  NOTIMPLEMENTED();
  return NULL;
}

AudioInputStream* AudioManagerOpenBSD::MakeAudioInputStream(
    const AudioParameters& params) {
  NOTIMPLEMENTED();
  return NULL;
}

AudioManagerOpenBSD::AudioManagerOpenBSD() {
}

AudioManagerOpenBSD::~AudioManagerOpenBSD() {
}

void AudioManagerOpenBSD::MuteAll() {
  NOTIMPLEMENTED();
}

void AudioManagerOpenBSD::UnMuteAll() {
  NOTIMPLEMENTED();
}

bool AudioManagerOpenBSD::IsRecordingInProgress() {
  NOTIMPLEMENTED();
  return false;
}

// static
AudioManager* AudioManager::CreateAudioManager() {
  return new AudioManagerOpenBSD();
}
