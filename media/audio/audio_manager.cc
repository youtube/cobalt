// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager.h"

#include "base/at_exit.h"
#include "base/logging.h"

namespace {

AudioManager* g_audio_manager = NULL;

// NullAudioManager is the audio manager used on the systems that have no
// audio support.
class NullAudioManager : public AudioManager {
 public:
  NullAudioManager() { }

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices() { return false; }
  virtual bool HasAudioInputDevices() { return false; }
  virtual AudioOutputStream* MakeAudioOutputStream(Format format, int channels,
                                                   int sample_rate,
                                                   char bits_per_sample) {
    NOTIMPLEMENTED();
    return NULL;
  }
  virtual AudioInputStream* MakeAudioInputStream(Format format, int channels,
                                                 int sample_rate,
                                                 char bits_per_sample,
                                                 uint32 samples_per_packet) {
    NOTIMPLEMENTED();
    return NULL;
  }
  virtual void MuteAll() { NOTIMPLEMENTED(); }
  virtual void UnMuteAll() { NOTIMPLEMENTED(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(NullAudioManager);
};

}  // namespace

// static
void AudioManager::Destroy(void* not_used) {
  delete g_audio_manager;
  g_audio_manager = NULL;
}

// static
AudioManager* AudioManager::GetAudioManager() {
  if (!g_audio_manager) {
    g_audio_manager = CreateAudioManager();
    g_audio_manager->Init();
    base::AtExitManager::RegisterCallback(&AudioManager::Destroy, NULL);
  }
  return g_audio_manager;
}
