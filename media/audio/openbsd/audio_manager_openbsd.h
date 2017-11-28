// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_OPENBSD_AUDIO_MANAGER_OPENBSD_H_
#define MEDIA_AUDIO_OPENBSD_AUDIO_MANAGER_OPENBSD_H_

#include <set>

#include "base/compiler_specific.h"
#include "media/audio/audio_manager_base.h"

namespace media {

class MEDIA_EXPORT AudioManagerOpenBSD : public AudioManagerBase {
 public:
  AudioManagerOpenBSD();

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
  virtual ~AudioManagerOpenBSD();

 private:
  // Called by MakeLinearOutputStream and MakeLowLatencyOutputStream.
  AudioOutputStream* MakeOutputStream(const AudioParameters& params);

  DISALLOW_COPY_AND_ASSIGN(AudioManagerOpenBSD);
};

}  // namespace media

#endif  // MEDIA_AUDIO_OPENBSD_AUDIO_MANAGER_OPENBSD_H_
