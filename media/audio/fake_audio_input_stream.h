// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A fake implementation of AudioInputStream, useful for testing purpose.

#ifndef MEDIA_AUDIO_FAKE_AUDIO_INPUT_STREAM_H_
#define MEDIA_AUDIO_FAKE_AUDIO_INOUT_STREAM_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

class FakeAudioInputStream
    : public AudioInputStream,
      public base::RefCountedThreadSafe<FakeAudioInputStream> {
 public:
  static AudioInputStream* MakeFakeStream(const AudioParameters& params);

  virtual bool Open() OVERRIDE;
  virtual void Start(AudioInputCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Close() OVERRIDE;

 private:
  // Give RefCountedThreadSafe access our destructor.
  friend class base::RefCountedThreadSafe<FakeAudioInputStream>;

  FakeAudioInputStream(const AudioParameters& params);
  virtual ~FakeAudioInputStream();

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
