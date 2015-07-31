// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_IOS_AUDIO_MANAGER_IOS_H_
#define MEDIA_AUDIO_IOS_AUDIO_MANAGER_IOS_H_

#include "base/basictypes.h"
#include "media/audio/audio_manager_base.h"

namespace media {

class PCMQueueInAudioInputStream;
class PCMQueueOutAudioOutputStream;

// iOS implementation of the AudioManager singleton. Supports only audio input.
class MEDIA_EXPORT AudioManagerIOS : public AudioManagerBase {
 public:
  AudioManagerIOS();

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices() OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;
  virtual AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioInputStream* MakeAudioInputStream(
      const AudioParameters& params, const std::string& device_id) OVERRIDE;

  // Implementation of AudioManagerBase.
  virtual AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params, const std::string& device_id) OVERRIDE;
  virtual AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params, const std::string& device_id) OVERRIDE;
  virtual void ReleaseOutputStream(AudioOutputStream* stream) OVERRIDE;
  virtual void ReleaseInputStream(AudioInputStream* stream) OVERRIDE;

 protected:
  virtual ~AudioManagerIOS();

 private:
  // Initializes the audio session if necessary. Safe to call multiple times.
  // Returns a bool indicating whether the audio session has been successfully
  // initialized (either in the current call or in a previous call).
  bool InitAudioSession();

  DISALLOW_COPY_AND_ASSIGN(AudioManagerIOS);
};

}  // namespace media

#endif  // MEDIA_AUDIO_IOS_AUDIO_MANAGER_IOS_H_
