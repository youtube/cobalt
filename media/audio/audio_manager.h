// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_MANAGER_H_
#define MEDIA_AUDIO_AUDIO_MANAGER_H_

#include "base/basictypes.h"
#include "media/audio/audio_parameters.h"

class AudioInputStream;
class AudioOutputStream;
class MessageLoop;

// Manages all audio resources. In particular it owns the AudioOutputStream
// objects. Provides some convenience functions that avoid the need to provide
// iterators over the existing streams.
class AudioManager {
 public:
  // Returns true if the OS reports existence of audio devices. This does not
  // guarantee that the existing devices support all formats and sample rates.
  virtual bool HasAudioOutputDevices() = 0;

  // Returns true if the OS reports existence of audio recording devices. This
  // does not guarantee that the existing devices support all formats and
  // sample rates.
  virtual bool HasAudioInputDevices() = 0;

  // Factory for all the supported stream formats. The |channels| can be 1 to 5.
  // The |sample_rate| is in hertz and can be any value supported by the
  // platform. For some future formats the |sample_rate| and |bits_per_sample|
  // can take special values.
  // Returns NULL if the combination of the parameters is not supported, or if
  // we have reached some other platform specific limit.
  //
  // AUDIO_PCM_LOW_LATENCY can be passed to this method and it has two effects:
  // 1- Instead of triple buffered the audio will be double buffered.
  // 2- A low latency driver or alternative audio subsystem will be used when
  //    available.
  //
  // Do not free the returned AudioOutputStream. It is owned by AudioManager.
  virtual AudioOutputStream* MakeAudioOutputStream(AudioParameters params) = 0;

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
  virtual AudioInputStream* MakeAudioInputStream(AudioParameters params,
                                                 int samples_per_packet) = 0;

  // Muting continues playback but effectively the volume is set to zero.
  // Un-muting returns the volume to the previous level.
  virtual void MuteAll() = 0;
  virtual void UnMuteAll() = 0;

  // Returns message loop used for audio IO.
  virtual MessageLoop* GetMessageLoop() = 0;

  // Get AudioManager singleton.
  // TODO(cpu): Define threading requirements for interacting with AudioManager.
  static AudioManager* GetAudioManager();

 protected:
  virtual ~AudioManager() {}

  // Called from GetAudioManager() to initialiaze the instance.
  virtual void Init() = 0;

 private:
  static void Destroy(void*);

  // Called by GetAudioManager() to create platform-specific audio manager.
  static AudioManager* CreateAudioManager();
};

#endif  // MEDIA_AUDIO_AUDIO_MANAGER_H_
