// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_
#define MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_

#include "base/thread.h"
#include "media/audio/audio_manager.h"

// AudioManagerBase provides AudioManager functions common for all platforms.
class AudioManagerBase : public AudioManager {
 public:
  AudioManagerBase();

  virtual void Init();

  virtual MessageLoop* GetMessageLoop();

 protected:
  virtual ~AudioManagerBase() {}

  bool initialized() { return initialized_; }

  // Thread used to interact with AudioOutputStreams created by this
  // audio manger.
  base::Thread audio_thread_;

  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerBase);
};

#endif  // MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_
