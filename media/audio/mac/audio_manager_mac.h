// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_

#include "base/basictypes.h"
#include "media/audio/audio_manager_base.h"

class PCMQueueInAudioInputStream;
class PCMQueueOutAudioOutputStream;

// Mac OS X implementation of the AudioManager singleton. This class is internal
// to the audio output and only internal users can call methods not exposed by
// the AudioManager class.
class AudioManagerMac : public AudioManagerBase {
 public:
  AudioManagerMac() {};

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices();
  virtual bool HasAudioInputDevices();
  virtual AudioOutputStream* MakeAudioOutputStream(AudioParameters params);
  virtual AudioInputStream* MakeAudioInputStream(AudioParameters params,
                                                 uint32 samples_per_packet);
  virtual void MuteAll();
  virtual void UnMuteAll();

  // Mac-only method to free the streams created by above facoty methods.
  // They are called internally by the respective audio stream when it has
  // been closed.
  void ReleaseOutputStream(PCMQueueOutAudioOutputStream* stream);
  void ReleaseInputStream(PCMQueueInAudioInputStream* stream);

 private:
  friend void DestroyAudioManagerMac(void*);
  virtual ~AudioManagerMac() {};
  DISALLOW_COPY_AND_ASSIGN(AudioManagerMac);
};

#endif  // MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_
