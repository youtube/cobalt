// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_LINUX_AUDIO_MANAGER_LINUX_H_
#define MEDIA_AUDIO_LINUX_AUDIO_MANAGER_LINUX_H_

#include <set>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "media/audio/audio_manager_base.h"

class AlsaWrapper;

class MEDIA_EXPORT AudioManagerLinux : public AudioManagerBase {
 public:
  AudioManagerLinux();

  // Call before using a newly created AudioManagerLinux instance.
  virtual void Init() OVERRIDE;

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices() OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;
  virtual AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioInputStream* MakeAudioInputStream(
      const AudioParameters& params, const std::string& device_id) OVERRIDE;
  virtual bool CanShowAudioInputSettings() OVERRIDE;
  virtual void ShowAudioInputSettings() OVERRIDE;
  virtual void GetAudioInputDeviceNames(media::AudioDeviceNames* device_names)
      OVERRIDE;

  virtual void MuteAll() OVERRIDE;
  virtual void UnMuteAll() OVERRIDE;

  virtual void ReleaseOutputStream(AudioOutputStream* stream);

 protected:
  virtual ~AudioManagerLinux();

 private:
  enum StreamType {
    kStreamPlayback = 0,
    kStreamCapture,
  };

  // Gets a list of available ALSA input devices.
  void GetAlsaAudioInputDevices(media::AudioDeviceNames* device_names);

  // Gets the ALSA devices' names and ids.
  void GetAlsaDevicesInfo(void** hint, media::AudioDeviceNames* device_names);

  // Checks if the specific ALSA device is available.
  bool IsAlsaDeviceAvailable(const char* device_name);

  // Returns true if a device is present for the given stream type.
  bool HasAnyAlsaAudioDevice(StreamType stream);

  scoped_ptr<AlsaWrapper> wrapper_;

  std::set<AudioOutputStream*> active_streams_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerLinux);
};

#endif  // MEDIA_AUDIO_LINUX_AUDIO_MANAGER_LINUX_H_
