// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_PARAMETERS_H_
#define MEDIA_AUDIO_AUDIO_PARAMETERS_H_

#include "base/basictypes.h"

struct AudioParameters {
  enum Format {
    AUDIO_PCM_LINEAR = 0,     // PCM is 'raw' amplitude samples.
    AUDIO_PCM_LOW_LATENCY,    // Linear PCM, low latency requested.
    AUDIO_MOCK,               // Creates a dummy AudioOutputStream object.
    AUDIO_LAST_FORMAT         // Only used for validation of format.
  };

  // Telephone quality sample rate, mostly for speech-only audio.
  static const uint32 kTelephoneSampleRate = 8000;
  // CD sampling rate is 44.1 KHz or conveniently 2x2x3x3x5x5x7x7.
  static const uint32 kAudioCDSampleRate = 44100;
  // Digital Audio Tape sample rate.
  static const uint32 kAudioDATSampleRate = 48000;

  AudioParameters();

  AudioParameters(Format format, int channels,
                  int sample_rate, int bits_per_sample);

  bool IsValid() const;

  Format format;        // Format of the stream.
  int channels;         // Number of channels.
  int sample_rate;      // Sampling frequency/rate.
  int bits_per_sample;  // Number of bits per sample.
};

#endif  // MEDIA_AUDIO_AUDIO_PARAMETERS_H_
