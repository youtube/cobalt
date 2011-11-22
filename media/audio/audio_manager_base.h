// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_
#define MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/threading/thread.h"
#include "media/audio/audio_manager.h"

class AudioOutputDispatcher;

// AudioManagerBase provides AudioManager functions common for all platforms.
class MEDIA_EXPORT AudioManagerBase : public AudioManager {
 public:
  // Name of the generic "default" device.
  static const char kDefaultDeviceName[];
  // Unique Id of the generic "default" device.
  static const char kDefaultDeviceId[];

  AudioManagerBase();

  virtual void Init() OVERRIDE;
  virtual void Cleanup() OVERRIDE;

  virtual MessageLoop* GetMessageLoop() OVERRIDE;

  virtual string16 GetAudioInputDeviceModel() OVERRIDE;

  virtual bool CanShowAudioInputSettings() OVERRIDE;
  virtual void ShowAudioInputSettings() OVERRIDE;

  virtual void GetAudioInputDeviceNames(media::AudioDeviceNames* device_names)
      OVERRIDE;

  virtual AudioOutputStream* MakeAudioOutputStreamProxy(
      const AudioParameters& params) OVERRIDE;

  virtual bool IsRecordingInProcess() OVERRIDE;

  void IncreaseActiveInputStreamCount();
  void DecreaseActiveInputStreamCount();

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

  // Counts the number of active input streams to find out if something else
  // is currently recording in Chrome.
  int num_active_input_streams_;

  // Lock used to synchronize the access to num_active_input_streams_.
  // Do not use for anything else and try to avoid using other locks
  // in this code whenever possible.
  base::Lock active_input_streams_lock_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerBase);
};

#endif  // MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_
