// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_parameters.h"

#include "media/base/limits.h"

namespace media {

AudioParameters::AudioParameters()
    : format_(AUDIO_PCM_LINEAR),
      channel_layout_(CHANNEL_LAYOUT_NONE),
      sample_rate_(0),
      bits_per_sample_(0),
      frames_per_buffer_(0),
      channels_(0) {
}

AudioParameters::AudioParameters(Format format, ChannelLayout channel_layout,
                                 int sample_rate, int bits_per_sample,
                                 int frames_per_buffer)
    : format_(format),
      channel_layout_(channel_layout),
      sample_rate_(sample_rate),
      bits_per_sample_(bits_per_sample),
      frames_per_buffer_(frames_per_buffer),
      channels_(ChannelLayoutToChannelCount(channel_layout)) {
}

void AudioParameters::Reset(Format format, ChannelLayout channel_layout,
                            int sample_rate, int bits_per_sample,
                            int frames_per_buffer) {
  format_ = format;
  channel_layout_ = channel_layout;
  sample_rate_ = sample_rate;
  bits_per_sample_ = bits_per_sample;
  frames_per_buffer_ = frames_per_buffer;
  channels_ = ChannelLayoutToChannelCount(channel_layout);
}

bool AudioParameters::IsValid() const {
  return (format_ >= 0) && (format_ < AUDIO_LAST_FORMAT) &&
      (channels_ > 0) && (channels_ <= media::limits::kMaxChannels) &&
      (sample_rate_ > 0) &&
      (sample_rate_ <= media::limits::kMaxSampleRate) &&
      (bits_per_sample_ > 0) &&
      (bits_per_sample_ <= media::limits::kMaxBitsPerSample) &&
      (frames_per_buffer_ > 0) &&
      (frames_per_buffer_ <= media::limits::kMaxSamplesPerPacket);
}

int AudioParameters::GetBytesPerBuffer() const {
  return frames_per_buffer_ * GetBytesPerFrame();
}

int AudioParameters::GetBytesPerSecond() const {
  return sample_rate_ * GetBytesPerFrame();
}

int AudioParameters::GetBytesPerFrame() const {
  return channels_ * bits_per_sample_ / 8;
}

bool AudioParameters::Compare::operator()(
    const AudioParameters& a,
    const AudioParameters& b) const {
  if (a.format_ < b.format_)
    return true;
  if (a.format_ > b.format_)
    return false;
  if (a.channels_ < b.channels_)
    return true;
  if (a.channels_ > b.channels_)
    return false;
  if (a.sample_rate_ < b.sample_rate_)
    return true;
  if (a.sample_rate_ > b.sample_rate_)
    return false;
  if (a.bits_per_sample_ < b.bits_per_sample_)
    return true;
  if (a.bits_per_sample_ > b.bits_per_sample_)
    return false;
  return a.frames_per_buffer_ < b.frames_per_buffer_;
}

}  // namespace media
