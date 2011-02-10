// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager.h"

#include "base/at_exit.h"
#include "base/compiler_specific.h"
#include "base/logging.h"

namespace {

bool g_destroy_called = false;
AudioManager* g_audio_manager = NULL;

// NullAudioManager is the audio manager used on the systems that have no
// audio support.
class NullAudioManager : public AudioManager {
 public:
  NullAudioManager() { }

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices() OVERRIDE { return false; }
  virtual bool HasAudioInputDevices() OVERRIDE { return false; }
  virtual AudioOutputStream* MakeAudioOutputStream(
      AudioParameters params) OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }
  virtual AudioInputStream* MakeAudioInputStream(
      AudioParameters params) OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }
  virtual void MuteAll() OVERRIDE { NOTIMPLEMENTED(); }
  virtual void UnMuteAll() OVERRIDE { NOTIMPLEMENTED(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(NullAudioManager);
};

}  // namespace

// static
void AudioManager::Destroy(void* not_used) {
  g_destroy_called = true;

  g_audio_manager->Cleanup();

  AudioManager* audio_manager = g_audio_manager;
  g_audio_manager = NULL;
  delete audio_manager;
}

// static
AudioManager* AudioManager::GetAudioManager() {
  if (!g_audio_manager && !g_destroy_called) {
    g_audio_manager = CreateAudioManager();
    g_audio_manager->Init();
    base::AtExitManager::RegisterCallback(&AudioManager::Destroy, NULL);
  }
  return g_audio_manager;
}
