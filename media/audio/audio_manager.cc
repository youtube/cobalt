// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop.h"

namespace media {

// Forward declaration of the platform specific AudioManager factory function.
AudioManager* CreateAudioManager();

AudioManager::AudioManager() {
}

AudioManager::~AudioManager() {
}

// static
AudioManager* AudioManager::Create() {
  return CreateAudioManager();
}

}  // namespace media
