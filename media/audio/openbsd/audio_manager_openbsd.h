// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_OPENBSD_AUDIO_MANAGER_OPENBSD_H_
#define MEDIA_AUDIO_OPENBSD_AUDIO_MANAGER_OPENBSD_H_

#include "media/audio/audio_io.h"

class AudioManagerOpenBSD : public AudioManagerBase {
 public:
  AudioManagerOpenBSD();

  // Call before using a newly created AudioManagerOpenBSD instance.
  virtual void Init();

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices();
  virtual bool HasAudioInputDevices();
  virtual AudioOutputStream* MakeAudioOutputStream(AudioParameters params);
  virtual AudioInputStream* MakeAudioInputStream(AudioParameters params,
                                                 uint32 samples_per_packet);
  virtual void MuteAll();
  virtual void UnMuteAll();

 protected:
  virtual ~AudioManagerOpenBSD();

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioManagerOpenBSD);
};

#endif  // MEDIA_AUDIO_OPENBSD_AUDIO_MANAGER_OPENBSD_H_
