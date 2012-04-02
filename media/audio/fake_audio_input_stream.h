// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A fake implementation of AudioInputStream, useful for testing purpose.

#ifndef MEDIA_AUDIO_FAKE_AUDIO_INPUT_STREAM_H_
#define MEDIA_AUDIO_FAKE_AUDIO_INOUT_STREAM_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "media/audio/audio_io.h"

namespace media {

class AudioManagerBase;
class AudioParameters;

class MEDIA_EXPORT FakeAudioInputStream
    : public AudioInputStream {
 public:
  static AudioInputStream* MakeFakeStream(AudioManagerBase* manager,
                                          const AudioParameters& params);

  virtual bool Open() OVERRIDE;
  virtual void Start(AudioInputCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual double GetMaxVolume() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual double GetVolume() OVERRIDE;
  virtual void SetAutomaticGainControl(bool enabled) OVERRIDE;
  virtual bool GetAutomaticGainControl() OVERRIDE;

 private:
  FakeAudioInputStream(AudioManagerBase* manager,
                       const AudioParameters& params);

  virtual ~FakeAudioInputStream();

  void DoCallback();

  AudioManagerBase* audio_manager_;
  AudioInputCallback* callback_;
  scoped_array<uint8> buffer_;
  int buffer_size_;
  base::Thread thread_;
  base::Time last_callback_time_;
  base::TimeDelta callback_interval_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioInputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_FAKE_AUDIO_INPUT_STREAM_H_
