// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_
#define MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_

#include <map>

#include "base/atomic_ref_count.h"
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

#ifndef NDEBUG
  // Overridden to make sure we don't accidentally get added reference counts on
  // the audio thread.  The AudioManagerBase instance must always be deleted
  // from outside the audio thread.
  virtual void AddRef() const OVERRIDE;
  virtual void Release() const OVERRIDE;
#endif

  virtual void Init() OVERRIDE;

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

  // Shuts down the audio thread and releases all the audio output dispatchers
  // on the audio thread.  All AudioOutputProxy instances should be freed before
  // Shutdown is called.
  void Shutdown();

 protected:
  virtual ~AudioManagerBase();

  typedef std::map<AudioParameters, scoped_refptr<AudioOutputDispatcher>,
                   AudioParameters::Compare>
      AudioOutputDispatchersMap;

  void ShutdownOnAudioThread();

  bool initialized() { return audio_thread_.IsRunning(); }

  // Thread used to interact with AudioOutputStreams created by this
  // audio manger.
  base::Thread audio_thread_;

  AudioOutputDispatchersMap output_dispatchers_;

  // Counts the number of active input streams to find out if something else
  // is currently recording in Chrome.
  base::AtomicRefCount num_active_input_streams_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerBase);
};

#endif  // MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_
