// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager_base.h"

AudioManagerBase::AudioManagerBase()
    : audio_thread_("AudioThread"),
      initialized_(false) {
}

void AudioManagerBase::Init() {
  initialized_ = audio_thread_.Start();
}

MessageLoop* AudioManagerBase::GetMessageLoop() {
  DCHECK(initialized_);
  return audio_thread_.message_loop();
}
