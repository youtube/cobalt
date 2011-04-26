// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_DECODER_CONFIG_H_
#define MEDIA_BASE_AUDIO_DECODER_CONFIG_H_

namespace media {

struct AudioDecoderConfig {
  AudioDecoderConfig(int bits, int channels, int rate)
      : bits_per_channel(bits),
        channels_per_sample(channels),
        sample_rate(rate) {
  }

  int bits_per_channel;
  int channels_per_sample;
  int sample_rate;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_DECODER_CONFIG_H_
