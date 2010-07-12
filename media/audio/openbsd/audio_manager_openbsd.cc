// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/openbsd/audio_manager_openbsd.h"

#include "base/at_exit.h"
#include "base/logging.h"

namespace {
AudioManagerOpenBSD* g_audio_manager = NULL;
}  // namespace

// Implementation of AudioManager.
bool AudioManagerOpenBSD::HasAudioOutputDevices() {
  NOTIMPLEMENTED();
  return false;
}

AudioOutputStream* AudioManagerOpenBSD::MakeAudioOutputStream(
    Format format,
    int channels,
    int sample_rate,
    char bits_per_sample) {
  NOTIMPLEMENTED();
  return NULL;
}

AudioManagerOpenBSD::AudioManagerOpenBSD() {
}

AudioManagerOpenBSD::~AudioManagerOpenBSD() {
}

void AudioManagerOpenBSD::Init() {
}

void AudioManagerOpenBSD::MuteAll() {
  NOTIMPLEMENTED();
}

void AudioManagerOpenBSD::UnMuteAll() {
  NOTIMPLEMENTED();
}

void DestroyAudioManagerOpenBSD(void* not_used) {
  delete g_audio_manager;
  g_audio_manager = NULL;
}

AudioManager* AudioManager::GetAudioManager() {
  if (!g_audio_manager) {
    g_audio_manager = new AudioManagerOpenBSD();
    g_audio_manager->Init();
    base::AtExitManager::RegisterCallback(&DestroyAudioManagerOpenBSD, NULL);
  }
  return g_audio_manager;
}
