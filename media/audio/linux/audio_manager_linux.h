// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_LINUX_AUDIO_MANAGER_LINUX_H_
#define MEDIA_AUDIO_LINUX_AUDIO_MANAGER_LINUX_H_

#include <set>

#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "media/audio/audio_manager_base.h"

class AlsaWrapper;

class MEDIA_EXPORT AudioManagerLinux : public AudioManagerBase {
 public:
  AudioManagerLinux();

  // Call before using a newly created AudioManagerLinux instance.
  virtual void Init();

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices();
  virtual bool HasAudioInputDevices();
  virtual AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params);
  virtual AudioInputStream* MakeAudioInputStream(const AudioParameters& params);
  virtual bool CanShowAudioInputSettings();
  virtual void ShowAudioInputSettings();
  virtual void GetAudioInputDeviceNames(media::AudioDeviceNames* device_names);

  virtual void MuteAll();
  virtual void UnMuteAll();

  virtual void ReleaseOutputStream(AudioOutputStream* stream);

 protected:
  virtual ~AudioManagerLinux();

 private:
  // Helper method to query if there is any valid input device
  bool HasAnyValidAudioInputDevice(void** hint);

  scoped_ptr<AlsaWrapper> wrapper_;

  std::set<AudioOutputStream*> active_streams_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerLinux);
};

#endif  // MEDIA_AUDIO_LINUX_AUDIO_MANAGER_LINUX_H_
