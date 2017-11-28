// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop_proxy.h"
#include "media/audio/audio_manager_base.h"

namespace media {

// Mac OS X implementation of the AudioManager singleton. This class is internal
// to the audio output and only internal users can call methods not exposed by
// the AudioManager class.
class MEDIA_EXPORT AudioManagerMac : public AudioManagerBase {
 public:
  AudioManagerMac();

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices() override;
  virtual bool HasAudioInputDevices() override;
  virtual void GetAudioInputDeviceNames(media::AudioDeviceNames* device_names)
      override;

  // Implementation of AudioManagerBase.
  virtual AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params) override;
  virtual AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params) override;
  virtual AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params, const std::string& device_id) override;
  virtual AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params, const std::string& device_id) override;
  virtual AudioParameters GetPreferredLowLatencyOutputStreamParameters(
      const AudioParameters& input_params) override;

  // Called by an internal device change listener.  Must be called on
  // |creating_message_loop_|.
  void OnDeviceChange();

 protected:
  virtual ~AudioManagerMac();

 private:
  bool listener_registered_;
  scoped_refptr<base::MessageLoopProxy> creating_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerMac);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_
