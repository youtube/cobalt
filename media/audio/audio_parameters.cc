// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_parameters.h"

#include "media/base/limits.h"

AudioParameters::AudioParameters()
    : format(AUDIO_PCM_LINEAR),
      channels(0),
      sample_rate(0),
      bits_per_sample(0) {
}

AudioParameters::AudioParameters(Format format, int channels,
                                 int sample_rate, int bits_per_sample)
    : format(format),
      channels(channels),
      sample_rate(sample_rate),
      bits_per_sample(bits_per_sample) {
}

bool AudioParameters::IsValid() const {
  return (format >= 0) && (format < AUDIO_LAST_FORMAT) &&
      (channels > 0) && (channels <= media::Limits::kMaxChannels) &&
      (sample_rate > 0) && (sample_rate <= media::Limits::kMaxSampleRate) &&
      (bits_per_sample > 0) &&
      (bits_per_sample <= media::Limits::kMaxBitsPerSample);
}
