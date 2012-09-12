// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_SIMPLE_SOURCES_H_
#define MEDIA_AUDIO_SIMPLE_SOURCES_H_

#include "base/synchronization/lock.h"
#include "media/audio/audio_io.h"
#include "media/base/seekable_buffer.h"

namespace media {

// An audio source that produces a pure sinusoidal tone.
class MEDIA_EXPORT SineWaveAudioSource
    : public AudioOutputStream::AudioSourceCallback {
 public:
  // |channels| is the number of audio channels, |freq| is the frequency in
  // hertz and it has to be less than half of the sampling frequency
  // |sample_freq| or else you will get aliasing.
  SineWaveAudioSource(int channels, double freq, double sample_freq);
  virtual ~SineWaveAudioSource() {}

  // Return up to |cap| samples of data via OnMoreData().  Use Reset() to
  // allow more data to be served.
  void CapSamples(int cap);
  void Reset();

  // Implementation of AudioSourceCallback.
  virtual int OnMoreData(AudioBus* audio_bus,
                         AudioBuffersState audio_buffers) OVERRIDE;
  virtual int OnMoreIOData(AudioBus* source,
                           AudioBus* dest,
                           AudioBuffersState audio_buffers) OVERRIDE;
  virtual void OnError(AudioOutputStream* stream, int code) OVERRIDE;

 protected:
  int channels_;
  double f_;
  int time_state_;
  int cap_;
  base::Lock time_lock_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_SIMPLE_SOURCES_H_
