// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_parameters.h"

#include "media/base/limits.h"

AudioParameters::AudioParameters()
    : format(AUDIO_PCM_LINEAR),
      channel_layout(CHANNEL_LAYOUT_NONE),
      sample_rate(0),
      bits_per_sample(0),
      samples_per_packet(0),
      channels(0) {
}

AudioParameters::AudioParameters(Format format, ChannelLayout channel_layout,
                                 int sample_rate, int bits_per_sample,
                                 int samples_per_packet)
    : format(format),
      channel_layout(channel_layout),
      sample_rate(sample_rate),
      bits_per_sample(bits_per_sample),
      samples_per_packet(samples_per_packet),
      channels(ChannelLayoutToChannelCount(channel_layout)) {
}

bool AudioParameters::IsValid() const {
  return (format >= 0) && (format < AUDIO_LAST_FORMAT) &&
      (channels > 0) && (channels <= media::Limits::kMaxChannels) &&
      (sample_rate > 0) && (sample_rate <= media::Limits::kMaxSampleRate) &&
      (bits_per_sample > 0) &&
      (bits_per_sample <= media::Limits::kMaxBitsPerSample) &&
      (samples_per_packet > 0) &&
      (samples_per_packet <= media::Limits::kMaxSamplesPerPacket);
}

int AudioParameters::GetPacketSize() const {
  return samples_per_packet * channels * bits_per_sample / 8;
}

int AudioParameters::GetBytesPerSecond() const {
  return sample_rate * channels * bits_per_sample / 8;
}

bool AudioParameters::Compare::operator()(
    const AudioParameters& a,
    const AudioParameters& b) const {
  if (a.format < b.format)
    return true;
  if (a.format > b.format)
    return false;
  if (a.channels < b.channels)
    return true;
  if (a.channels > b.channels)
    return false;
  if (a.sample_rate < b.sample_rate)
    return true;
  if (a.sample_rate > b.sample_rate)
    return false;
  if (a.bits_per_sample < b.bits_per_sample)
    return true;
  if (a.bits_per_sample > b.bits_per_sample)
    return false;
  return a.samples_per_packet < b.samples_per_packet;
}
