// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "media/audio/audio_manager_base.h"

// Mac OS X implementation of the AudioManager singleton. This class is internal
// to the audio output and only internal users can call methods not exposed by
// the AudioManager class.
class AudioManagerMac : public AudioManagerBase {
 public:
  AudioManagerMac();

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices() OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;
  virtual void GetAudioInputDeviceNames(media::AudioDeviceNames* device_names)
      OVERRIDE;
  virtual AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioInputStream* MakeAudioInputStream(
      const AudioParameters& params, const std::string& device_id) OVERRIDE;
  virtual void MuteAll() OVERRIDE;
  virtual void UnMuteAll() OVERRIDE;

  // Mac-only method to free the streams created by above facoty methods.
  // They are called internally by the respective audio stream when it has
  // been closed.
  void ReleaseOutputStream(AudioOutputStream* stream);
  void ReleaseInputStream(AudioInputStream* stream);

 private:
  virtual ~AudioManagerMac();

  // Number of currently open output streams.
  size_t num_output_streams_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerMac);
};

#endif  // MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_
