// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/openbsd/audio_manager_openbsd.h"

#include "base/logging.h"

static AudioManagerOpenBSD* g_audio_manager = NULL;

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
    AudioParameters params) {
  NOTIMPLEMENTED();
  return NULL;
}

AudioInputStream* AudioManagerOpenBSD::MakeAudioInputStream(
    AudioParameters params) {
  NOTIMPLEMENTED();
  return NULL;
}

AudioManagerOpenBSD::AudioManagerOpenBSD() {
}

AudioManagerOpenBSD::~AudioManagerOpenBSD() {
}

void AudioManagerOpenBSD::Init() {
  AudioManagerBase::Init();
}

void AudioManagerOpenBSD::MuteAll() {
  NOTIMPLEMENTED();
}

void AudioManagerOpenBSD::UnMuteAll() {
  NOTIMPLEMENTED();
}

// static
AudioManager* AudioManager::CreateAudioManager() {
  return new AudioManagerOpenBSD();
}
