// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/sample_rates.h"

namespace media {

AudioSampleRate AsAudioSampleRate(int sample_rate) {
  switch (sample_rate) {
    case 8000: return k8000Hz;
    case 16000: return k16000Hz;
    case 32000: return k32000Hz;
    case 48000: return k48000Hz;
    case 96000: return k96000Hz;
    case 11025: return k11025Hz;
    case 22050: return k22050Hz;
    case 44100: return k44100Hz;
    case 88200: return k88200Hz;
    case 176400: return k176400Hz;
    case 192000: return k192000Hz;
  }
  return kUnexpectedAudioSampleRate;
}

}  // namespace media
