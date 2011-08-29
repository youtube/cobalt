// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_
#define MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_

#include <map>

#include "base/threading/thread.h"
#include "media/audio/audio_manager.h"

class AudioOutputDispatcher;

// AudioManagerBase provides AudioManager functions common for all platforms.
class MEDIA_EXPORT AudioManagerBase : public AudioManager {
 public:
  // Name of the generic "default" device.
  static const char kDefaultDeviceName[];

  AudioManagerBase();

  virtual void Init();
  virtual void Cleanup();

  virtual MessageLoop* GetMessageLoop();

  virtual string16 GetAudioInputDeviceModel();

  virtual bool CanShowAudioInputSettings();
  virtual void ShowAudioInputSettings();

  virtual void GetAudioInputDeviceNames(media::AudioDeviceNames* device_names);

  virtual AudioOutputStream* MakeAudioOutputStreamProxy(
      const AudioParameters& params);

 protected:
  virtual ~AudioManagerBase();

  typedef std::map<AudioParameters, scoped_refptr<AudioOutputDispatcher>,
                   AudioParameters::Compare>
      AudioOutputDispatchersMap;

  bool initialized() { return initialized_; }

  // Thread used to interact with AudioOutputStreams created by this
  // audio manger.
  base::Thread audio_thread_;

  bool initialized_;

  AudioOutputDispatchersMap output_dispatchers_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerBase);
};

#endif  // MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_
