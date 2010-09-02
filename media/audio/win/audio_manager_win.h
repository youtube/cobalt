// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_MANAGER_WIN_H_
#define MEDIA_AUDIO_AUDIO_MANAGER_WIN_H_

#include <windows.h>

#include "base/basictypes.h"
#include "media/audio/audio_manager_base.h"

class PCMWaveInAudioInputStream;
class PCMWaveOutAudioOutputStream;

// Windows implementation of the AudioManager singleton. This class is internal
// to the audio output and only internal users can call methods not exposed by
// the AudioManager class.
class AudioManagerWin : public AudioManagerBase {
 public:
  AudioManagerWin() {}
  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices();
  virtual bool HasAudioInputDevices();
  virtual AudioOutputStream* MakeAudioOutputStream(AudioParameters params);
  virtual AudioInputStream* MakeAudioInputStream(AudioParameters params,
                                                 int samples_per_packet);
  virtual void MuteAll();
  virtual void UnMuteAll();

  // Windows-only methods to free a stream created in MakeAudioStream. These
  // are called internally by the audio stream when it has been closed.
  void ReleaseOutputStream(PCMWaveOutAudioOutputStream* stream);

  // Called internally by the audio stream when it has been closed.
  void ReleaseInputStream(PCMWaveInAudioInputStream* stream);

 private:
  friend void DestroyAudioManagerWin(void *);
  virtual ~AudioManagerWin();
  DISALLOW_COPY_AND_ASSIGN(AudioManagerWin);
};

#endif  // MEDIA_AUDIO_AUDIO_MANAGER_WIN_H_
