// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager.h"

#include "base/at_exit.h"
#include "base/logging.h"

static bool g_destroy_called = false;
static AudioManager* g_audio_manager = NULL;

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
