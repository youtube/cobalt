// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_AUDIO_MANAGER_WIN_H_
#define MEDIA_AUDIO_WIN_AUDIO_MANAGER_WIN_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "media/audio/audio_manager_base.h"

class PCMWaveOutAudioOutputStream;

// Windows implementation of the AudioManager singleton. This class is internal
// to the audio output and only internal users can call methods not exposed by
// the AudioManager class.
class AudioManagerWin : public AudioManagerBase {
 public:
  AudioManagerWin();
  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices() OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;
  virtual AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioInputStream* MakeAudioInputStream(
      const AudioParameters& params, const std::string& device_id) OVERRIDE;
  virtual void MuteAll() OVERRIDE;
  virtual void UnMuteAll() OVERRIDE;
  virtual string16 GetAudioInputDeviceModel() OVERRIDE;
  virtual bool CanShowAudioInputSettings() OVERRIDE;
  virtual void ShowAudioInputSettings() OVERRIDE;
  virtual void GetAudioInputDeviceNames(media::AudioDeviceNames* device_names)
      OVERRIDE;

  // Windows-only methods to free a stream created in MakeAudioStream. These
  // are called internally by the audio stream when it has been closed.
  void ReleaseOutputStream(AudioOutputStream* stream);

  // Called internally by the audio stream when it has been closed.
  void ReleaseInputStream(AudioInputStream* stream);

 private:
  virtual ~AudioManagerWin();

  // Number of currently open output streams.
  int num_output_streams_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerWin);
};

#endif  // MEDIA_AUDIO_WIN_AUDIO_MANAGER_WIN_H_
