// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A fake implementation of AudioInputStream, useful for testing purpose.

#ifndef MEDIA_AUDIO_FAKE_AUDIO_INPUT_STREAM_H_
#define MEDIA_AUDIO_FAKE_AUDIO_INOUT_STREAM_H_

#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "base/time.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

class FakeAudioInputStream :
    public AudioInputStream,
    public base::RefCountedThreadSafe<FakeAudioInputStream> {
 public:
  static AudioInputStream* MakeFakeStream(AudioParameters params,
                                          int samples_per_packet);

  virtual bool Open();
  virtual void Start(AudioInputCallback* callback);
  virtual void Stop();
  virtual void Close();

 private:
  // Give RefCountedThreadSafe access our destructor.
  friend class base::RefCountedThreadSafe<FakeAudioInputStream>;

  FakeAudioInputStream(AudioParameters params, int samples_per_packet);
  virtual ~FakeAudioInputStream() {}

  void DoCallback();

  AudioInputCallback* callback_;
  scoped_array<uint8> buffer_;
  int buffer_size_;
  base::Thread thread_;
  base::Time last_callback_time_;
  int callback_interval_ms_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioInputStream);
};

#endif  // MEDIA_AUDIO_FAKE_AUDIO_INPUT_STREAM_H_
