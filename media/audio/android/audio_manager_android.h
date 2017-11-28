// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_AUDIO_MANAGER_ANDROID_H_
#define MEDIA_AUDIO_ANDROID_AUDIO_MANAGER_ANDROID_H_

#include "media/audio/audio_manager_base.h"

namespace media {

// Android implemention of AudioManager.
class MEDIA_EXPORT AudioManagerAndroid : public AudioManagerBase {
 public:
  AudioManagerAndroid();

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices() override;
  virtual bool HasAudioInputDevices() override;

  // Implementation of AudioManagerBase.
  virtual AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params) override;
  virtual AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params) override;
  virtual AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params, const std::string& device_id) override;
  virtual AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params, const std::string& device_id) override;

 protected:
  virtual ~AudioManagerAndroid();

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioManagerAndroid);
};

}  // namespace media

#endif  // MEDIA_AUDIO_ANDROID_AUDIO_MANAGER_ANDROID_H_
