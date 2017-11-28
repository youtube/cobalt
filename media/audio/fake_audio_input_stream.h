// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A fake implementation of AudioInputStream, useful for testing purpose.

#ifndef MEDIA_AUDIO_FAKE_AUDIO_INPUT_STREAM_H_
#define MEDIA_AUDIO_FAKE_AUDIO_INOUT_STREAM_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

namespace media {

class AudioManagerBase;

class MEDIA_EXPORT FakeAudioInputStream
    : public AudioInputStream {
 public:
  static AudioInputStream* MakeFakeStream(AudioManagerBase* manager,
                                          const AudioParameters& params);

  virtual bool Open() override;
  virtual void Start(AudioInputCallback* callback) override;
  virtual void Stop() override;
  virtual void Close() override;
  virtual double GetMaxVolume() override;
  virtual void SetVolume(double volume) override;
  virtual double GetVolume() override;
  virtual void SetAutomaticGainControl(bool enabled) override;
  virtual bool GetAutomaticGainControl() override;

  // Generate one beep sound. This method is called by
  // FakeVideoCaptureDevice to test audio/video synchronization.
  // This is a static method because FakeVideoCaptureDevice is
  // disconnected from an audio device. This means only one instance of
  // this class gets to respond, which is okay because we assume there's
  // only one stream for this testing purpose.
  // TODO(hclam): Make this non-static. To do this we'll need to fix
  // crbug.com/159053 such that video capture device is aware of audio
  // input stream.
  static void BeepOnce();

 private:
  FakeAudioInputStream(AudioManagerBase* manager,
                       const AudioParameters& params);

  virtual ~FakeAudioInputStream();

  void DoCallback();

  AudioManagerBase* audio_manager_;
  AudioInputCallback* callback_;
  scoped_array<uint8> buffer_;
  int buffer_size_;
  AudioParameters params_;
  base::Thread thread_;
  base::Time last_callback_time_;
  base::TimeDelta callback_interval_;
  int beep_duration_in_buffers_;
  int beep_generated_in_buffers_;
  int beep_period_in_frames_;
  int frames_elapsed_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioInputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_FAKE_AUDIO_INPUT_STREAM_H_
