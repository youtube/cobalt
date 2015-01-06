// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_SAMPLE_RATES_H_
#define MEDIA_AUDIO_SAMPLE_RATES_H_

#include "media/base/media_export.h"

namespace media {

// Enumeration used for histogramming sample rates into distinct buckets.
// Logged to UMA, so never reuse a value, always add new/greater ones!
enum AudioSampleRate {
  k8000Hz = 0,
  k16000Hz = 1,
  k32000Hz = 2,
  k48000Hz = 3,
  k96000Hz = 4,
  k11025Hz = 5,
  k22050Hz = 6,
  k44100Hz = 7,
  k88200Hz = 8,
  k176400Hz = 9,
  k192000Hz = 10,
  kUnexpectedAudioSampleRate  // Must always be last!
};

// Helper method to convert integral values to their respective enum values,
// or kUnexpectedAudioSampleRate if no match exists.
MEDIA_EXPORT AudioSampleRate AsAudioSampleRate(int sample_rate);

}  // namespace media

#endif  // MEDIA_AUDIO_SAMPLE_RATES_H_
