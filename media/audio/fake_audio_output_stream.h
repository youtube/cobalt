// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A fake implementation of AudioOutputStream. It is used for testing purpose.
// TODO(hclam): Implement a thread in this fake output stream to simulate an
// audio output stream reading from AudioSourceCallback.

#ifndef MEDIA_AUDIO_FAKE_AUDIO_OUTPUT_STREAM_H_
#define MEDIA_AUDIO_FAKE_AUDIO_OUTOUT_STREAM_H_

#include <vector>

#include "base/scoped_ptr.h"
#include "media/audio/audio_output.h"

class FakeAudioOutputStream : public AudioOutputStream {
 public:
  static AudioOutputStream* MakeFakeStream();
  static FakeAudioOutputStream* GetLastFakeStream();

  virtual bool Open(size_t packet_size);
  virtual void Start(AudioSourceCallback* callback);
  virtual void Stop();
  virtual void SetVolume(double volume);
  virtual void GetVolume(double* volume);
  virtual void Close();

  char* buffer() { return buffer_.get(); }
  double volume() { return volume_; }

 private:
  FakeAudioOutputStream();
  virtual ~FakeAudioOutputStream() {}

  static void DestroyLastFakeStream(void* param);
  static bool has_created_fake_stream_;
  static FakeAudioOutputStream* last_fake_stream_;

  double volume_;
  AudioSourceCallback* callback_;
  scoped_array<char> buffer_;
  size_t packet_size_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioOutputStream);
};

#endif  // MEDIA_AUDIO_FAKE_AUDIO_OUTPUT_STREAM_H_

