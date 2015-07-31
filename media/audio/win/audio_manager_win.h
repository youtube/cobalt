// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_AUDIO_MANAGER_WIN_H_
#define MEDIA_AUDIO_WIN_AUDIO_MANAGER_WIN_H_

#include <string>

#include "media/audio/audio_manager_base.h"

namespace media {

class AudioDeviceListenerWin;

// Windows implementation of the AudioManager singleton. This class is internal
// to the audio output and only internal users can call methods not exposed by
// the AudioManager class.
class MEDIA_EXPORT AudioManagerWin : public AudioManagerBase {
 public:
  AudioManagerWin();

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices() OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;
  virtual string16 GetAudioInputDeviceModel() OVERRIDE;
  virtual bool CanShowAudioInputSettings() OVERRIDE;
  virtual void ShowAudioInputSettings() OVERRIDE;
  virtual void GetAudioInputDeviceNames(media::AudioDeviceNames* device_names)
      OVERRIDE;

  // Implementation of AudioManagerBase.
  virtual AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params, const std::string& device_id) OVERRIDE;
  virtual AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params, const std::string& device_id) OVERRIDE;
  virtual AudioParameters GetPreferredLowLatencyOutputStreamParameters(
      const AudioParameters& input_params) OVERRIDE;

 protected:
  virtual ~AudioManagerWin();

 private:
  enum EnumerationType {
    kUninitializedEnumeration = 0,
    kMMDeviceEnumeration,
    kWaveEnumeration,
  };

  // Allow unit test to modify the utilized enumeration API.
  friend class AudioInputDeviceTest;

  EnumerationType enumeration_type_;
  EnumerationType enumeration_type() { return enumeration_type_; }
  void SetEnumerationType(EnumerationType type) {
    enumeration_type_ = type;
  }

  // Returns a PCMWaveInAudioInputStream instance or NULL on failure.
  // This method converts MMDevice-style device ID to WaveIn-style device ID if
  // necessary.
  // (Please see device_enumeration_win.h for more info about the two kinds of
  // device IDs.)
  AudioInputStream* CreatePCMWaveInAudioInputStream(
      const AudioParameters& params,
      const std::string& device_id);

  // Helper methods for constructing AudioDeviceListenerWin on the audio thread.
  void CreateDeviceListener();
  void DestroyDeviceListener();

  // Listen for output device changes.
  scoped_ptr<AudioDeviceListenerWin> output_device_listener_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerWin);
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_AUDIO_MANAGER_WIN_H_
