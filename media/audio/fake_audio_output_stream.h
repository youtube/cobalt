// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

namespace media {

class AudioManagerBase;

class MEDIA_EXPORT FakeAudioOutputStream : public AudioOutputStream {
 public:
  static AudioOutputStream* MakeFakeStream(AudioManagerBase* manager,
                                           const AudioParameters& params);

  static FakeAudioOutputStream* GetCurrentFakeStream();

  virtual bool Open() OVERRIDE;
  virtual void Start(AudioSourceCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual void GetVolume(double* volume) OVERRIDE;
  virtual void Close() OVERRIDE;

  AudioBus* audio_bus() { return audio_bus_.get(); }

 private:
  explicit FakeAudioOutputStream(AudioManagerBase* manager,
                                 const AudioParameters& params);

  virtual ~FakeAudioOutputStream();

  static FakeAudioOutputStream* current_fake_stream_;

  AudioManagerBase* audio_manager_;
  double volume_;
  AudioSourceCallback* callback_;
  scoped_ptr<AudioBus> audio_bus_;
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioOutputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_FAKE_AUDIO_OUTPUT_STREAM_H_
