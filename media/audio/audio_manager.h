// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_MANAGER_H_
#define MEDIA_AUDIO_AUDIO_MANAGER_H_

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/task.h"
#include "media/audio/audio_device_name.h"
#include "media/audio/audio_parameters.h"

class AudioInputStream;
class AudioOutputStream;
class MessageLoop;

// TODO(sergeyu): In this interface and some other places AudioParameters struct
// is passed by value. It is better to change it to const reference.

// Manages all audio resources. In particular it owns the AudioOutputStream
// objects. Provides some convenience functions that avoid the need to provide
// iterators over the existing streams.
class MEDIA_EXPORT AudioManager {
 public:
  // Returns true if the OS reports existence of audio devices. This does not
  // guarantee that the existing devices support all formats and sample rates.
  virtual bool HasAudioOutputDevices() = 0;

  // Returns true if the OS reports existence of audio recording devices. This
  // does not guarantee that the existing devices support all formats and
  // sample rates.
  virtual bool HasAudioInputDevices() = 0;

  // Returns a human readable string for the model/make of the active audio
  // input device for this computer.
  virtual string16 GetAudioInputDeviceModel() = 0;

  // Returns true if the platform specific audio input settings UI is known
  // and can be shown.
  virtual bool CanShowAudioInputSettings() = 0;

  // Opens the platform default audio input settings UI.
  // Note: This could invoke an external application/preferences pane, so
  // ideally must not be called from the UI thread or other time sensitive
  // threads to avoid blocking the rest of the application.
  virtual void ShowAudioInputSettings() = 0;

  // Appends a list of available input devices. It is not guaranteed that
  // all the devices in the list support all formats and sample rates for
  // recording.
  virtual void GetAudioInputDeviceNames(
      media::AudioDeviceNames* device_names) = 0;

  // Factory for all the supported stream formats. |params| defines parameters
  // of the audio stream to be created.
  //
  // |params.sample_per_packet| is the requested buffer allocation which the
  // audio source thinks it can usually fill without blocking. Internally two
  // or three buffers are created, one will be locked for playback and one will
  // be ready to be filled in the call to AudioSourceCallback::OnMoreData().
  //
  // Returns NULL if the combination of the parameters is not supported, or if
  // we have reached some other platform specific limit.
  //
  // |params.format| can be set to AUDIO_PCM_LOW_LATENCY and that has two
  // effects:
  // 1- Instead of triple buffered the audio will be double buffered.
  // 2- A low latency driver or alternative audio subsystem will be used when
  //    available.
  //
  // Do not free the returned AudioOutputStream. It is owned by AudioManager.
  virtual AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params) = 0;

  // Creates new audio output proxy. A proxy implements
  // AudioOutputStream interface, but unlike regular output stream
  // created with MakeAudioOutputStream() it opens device only when a
  // sound is actually playing.
  virtual AudioOutputStream* MakeAudioOutputStreamProxy(
      const AudioParameters& params) = 0;

  // Factory to create audio recording streams.
  // |channels| can be 1 or 2.
  // |sample_rate| is in hertz and can be any value supported by the platform.
  // |bits_per_sample| can be any value supported by the platform.
  // |samples_per_packet| is in hertz as well and can be 0 to |sample_rate|,
  // with 0 suggesting that the implementation use a default value for that
  // platform.
  // Returns NULL if the combination of the parameters is not supported, or if
  // we have reached some other platform specific limit.
  //
  // Do not free the returned AudioInputStream. It is owned by AudioManager.
  // When you are done with it, call |Stop()| and |Close()| to release it.
  virtual AudioInputStream* MakeAudioInputStream(
      const AudioParameters& params, const std::string& device_id) = 0;

  // Muting continues playback but effectively the volume is set to zero.
  // Un-muting returns the volume to the previous level.
  virtual void MuteAll() = 0;
  virtual void UnMuteAll() = 0;

  // Used to determine if something else is currently making use of audio input.
  virtual bool IsRecordingInProcess() = 0;

  // Returns message loop used for audio IO.
  virtual MessageLoop* GetMessageLoop() = 0;

  // Get AudioManager singleton.
  // TODO(cpu): Define threading requirements for interacting with AudioManager.
  static AudioManager* GetAudioManager();

 protected:
  virtual ~AudioManager() {}

  // Called from GetAudioManager() to initialiaze the instance.
  virtual void Init() = 0;

  // Called by Destroy() to cleanup the instance before destruction.
  virtual void Cleanup() = 0;

 private:
  // Allow unit tests to delete and recreate the singleton.
  friend class AutoAudioManagerCleanup;
  static void Destroy(void*);
  static bool SingletonExists();
  static void Resurrect();

  // Called by GetAudioManager() to create platform-specific audio manager.
  static AudioManager* CreateAudioManager();
};

DISABLE_RUNNABLE_METHOD_REFCOUNT(AudioManager);

#endif  // MEDIA_AUDIO_AUDIO_MANAGER_H_
