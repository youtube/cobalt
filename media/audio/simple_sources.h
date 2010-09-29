// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_SIMPLE_SOURCES_H_
#define MEDIA_AUDIO_SIMPLE_SOURCES_H_

#include <list>

#include "base/lock.h"
#include "media/audio/audio_io.h"
#include "media/base/seekable_buffer.h"

// An audio source that produces a pure sinusoidal tone.
class SineWaveAudioSource : public AudioOutputStream::AudioSourceCallback {
 public:
  enum Format {
    FORMAT_8BIT_LINEAR_PCM,
    FORMAT_16BIT_LINEAR_PCM,
  };
  // |channels| is the number of audio channels, |freq| is the frequency in
  // hertz and it has to be less than half of the sampling frequency
  // |sample_freq| or else you will get aliasing.
  SineWaveAudioSource(Format format, int channels,
                      double freq, double sample_freq);
  virtual ~SineWaveAudioSource() {}

  // Implementation of AudioSourceCallback.
  virtual uint32 OnMoreData(
      AudioOutputStream* stream, uint8* dest, uint32 max_size,
      AudioBuffersState audio_buffers);
  virtual void OnClose(AudioOutputStream* stream);
  virtual void OnError(AudioOutputStream* stream, int code);

 protected:
  Format format_;
  int channels_;
  double freq_;
  double sample_freq_;
};

// Defines an interface for pushing audio output. In contrast, the interfaces
// defined by AudioSourceCallback are pull model only.
class PushAudioOutput {
 public:
  virtual ~PushAudioOutput(){}

  // Write audio data to the audio device. It will be played eventually.
  // Returns false on failure.
  virtual bool Write(const void* data, uint32 len) = 0;

  // Returns the number of bytes that have been buffered but not yet given
  // to the audio device.
  virtual uint32 UnProcessedBytes() = 0;
};

// A fairly basic class to connect a push model provider PushAudioOutput to
// a pull model provider AudioSourceCallback. Fundamentally it manages a series
// of audio buffers and is unaware of the actual audio format.
// Note that the PushSource is not thread safe and user need to provide locking.
class PushSource : public AudioOutputStream::AudioSourceCallback,
                   public PushAudioOutput {
 public:
  PushSource();
  virtual ~PushSource();

  // Write one buffer.
  virtual bool Write(const void* data, uint32 len);

  // Return the total number of bytes not given to the audio device yet.
  virtual uint32 UnProcessedBytes();

  // Implementation of AudioSourceCallback.
  virtual uint32 OnMoreData(AudioOutputStream* stream, uint8* dest,
                            uint32 max_size, AudioBuffersState buffers_state);
  virtual void OnClose(AudioOutputStream* stream);
  virtual void OnError(AudioOutputStream* stream, int code);

  // Discard all buffered data and reset to initial state.
  void ClearAll();

 private:
  // Free acquired resources.
  void CleanUp();

  media::SeekableBuffer buffer_;
};

#endif  // MEDIA_AUDIO_SIMPLE_SOURCES_H_
