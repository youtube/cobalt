// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager.h"

#include "base/at_exit.h"
#include "base/logging.h"
#include "base/message_loop.h"

// Used only to make sure we never create more than one instance.
static AudioManager* g_audio_manager = NULL;

// Forward declaration of the platform specific AudioManager factory function.
AudioManager* CreateAudioManager();

AudioManager::AudioManager() {
  CHECK(g_audio_manager == NULL);
  g_audio_manager = this;
}

AudioManager::~AudioManager() {
  CHECK(g_audio_manager == this);
  g_audio_manager = NULL;
}

#ifndef NDEBUG
void AudioManager::AddRef() const {
  base::RefCountedThreadSafe<AudioManager>::AddRef();
}

void AudioManager::Release() const {
  base::RefCountedThreadSafe<AudioManager>::Release();
}
#endif

// static
scoped_refptr<AudioManager> AudioManager::Create() {
  AudioManager* ret = CreateAudioManager();
  DCHECK(ret == g_audio_manager);
  ret->Init();
  return ret;
}
