// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_LINUX_AUDIO_MANAGER_LINUX_H_
#define MEDIA_AUDIO_LINUX_AUDIO_MANAGER_LINUX_H_

#include <map>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "media/audio/audio_manager_base.h"

class AlsaPcmOutputStream;
class AlsaWrapper;

class AudioManagerLinux : public AudioManagerBase {
 public:
  AudioManagerLinux();

  // Call before using a newly created AudioManagerLinux instance.
  virtual void Init();

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices();
  virtual bool HasAudioInputDevices();
  virtual AudioOutputStream* MakeAudioOutputStream(AudioParameters params);
  virtual AudioInputStream* MakeAudioInputStream(AudioParameters params);

  virtual void MuteAll();
  virtual void UnMuteAll();

  virtual void ReleaseOutputStream(AlsaPcmOutputStream* stream);

 protected:
  virtual ~AudioManagerLinux();

 private:
  scoped_ptr<AlsaWrapper> wrapper_;

  base::Lock lock_;
  std::map<AlsaPcmOutputStream*, scoped_refptr<AlsaPcmOutputStream> >
      active_streams_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerLinux);
};

#endif  // MEDIA_AUDIO_LINUX_AUDIO_MANAGER_LINUX_H_
