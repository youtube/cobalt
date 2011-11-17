// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_OPENBSD_AUDIO_MANAGER_OPENBSD_H_
#define MEDIA_AUDIO_OPENBSD_AUDIO_MANAGER_OPENBSD_H_

#include <set>

#include "base/compiler_specific.h"
#include "media/audio/audio_manager_base.h"

class MEDIA_EXPORT AudioManagerOpenBSD : public AudioManagerBase {
 public:
  AudioManagerOpenBSD();

  // Call before using a newly created AudioManagerOpenBSD instance.
  virtual void Init() OVERRIDE;

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices() OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;
  virtual AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioInputStream* MakeAudioInputStream(const AudioParameters& params)
      OVERRIDE;

  virtual void MuteAll() OVERRIDE;
  virtual void UnMuteAll() OVERRIDE;

  virtual void ReleaseOutputStream(AudioOutputStream* stream);

 protected:
  virtual ~AudioManagerOpenBSD();

 private:
  std::set<AudioOutputStream*> active_streams_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerOpenBSD);
};

#endif  // MEDIA_AUDIO_OPENBSD_AUDIO_MANAGER_OPENBSD_H_
