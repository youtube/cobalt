// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A fake implementation of AudioOutputStream. It is used for testing purpose.
// TODO(hclam): Implement a thread in this fake output stream to simulate an
// audio output stream reading from AudioSourceCallback.

#ifndef MEDIA_AUDIO_FAKE_AUDIO_OUTPUT_STREAM_H_
#define MEDIA_AUDIO_FAKE_AUDIO_OUTOUT_STREAM_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

class MEDIA_EXPORT FakeAudioOutputStream : public AudioOutputStream {
 public:
  static AudioOutputStream* MakeFakeStream(const AudioParameters& params);
  static FakeAudioOutputStream* GetLastFakeStream();

  virtual bool Open();
  virtual void Start(AudioSourceCallback* callback);
  virtual void Stop();
  virtual void SetVolume(double volume);
  virtual void GetVolume(double* volume);
  virtual void Close();

  uint8* buffer() { return buffer_.get(); }
  double volume() { return volume_; }

 private:
  explicit FakeAudioOutputStream(const AudioParameters& params);
  virtual ~FakeAudioOutputStream();

  static void DestroyLastFakeStream(void* param);
  static bool has_created_fake_stream_;
  static FakeAudioOutputStream* last_fake_stream_;

  double volume_;
  AudioSourceCallback* callback_;
  scoped_array<uint8> buffer_;
  uint32 packet_size_;
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioOutputStream);
};

#endif  // MEDIA_AUDIO_FAKE_AUDIO_OUTPUT_STREAM_H_
