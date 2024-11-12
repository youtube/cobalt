// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_INTERLEAVED_CHANNEL_MIXER_H_
#define CHROMECAST_MEDIA_AUDIO_INTERLEAVED_CHANNEL_MIXER_H_

#include <vector>

#include "media/base/channel_layout.h"

namespace chromecast {
namespace media {

// Converts interleaved audio from one channel layout to another.
class InterleavedChannelMixer {
 public:
  InterleavedChannelMixer(::media::ChannelLayout input_layout,
                          int input_channel_count,
                          ::media::ChannelLayout output_layout,
                          int output_channel_count,
                          int max_frames);

  InterleavedChannelMixer(const InterleavedChannelMixer&) = delete;
  InterleavedChannelMixer& operator=(const InterleavedChannelMixer&) = delete;

  ~InterleavedChannelMixer();

  int input_channel_count() const { return input_channel_count_; }
  int output_channel_count() const { return output_channel_count_; }

  // Transforms the |input| audio from the input channel layout to the output
  // channel layout. Returns a pointer to the transformed data. Does not modify
  // the contents of |input|. The returned buffer is valid as long as |input|
  // is valid, or until |this| is destroyed, whichever is shorter.
  float* Transform(const float* input, int num_frames);

 private:
  const ::media::ChannelLayout input_layout_;
  const int input_channel_count_;
  const ::media::ChannelLayout output_layout_;
  const int output_channel_count_;
  const int max_frames_;

  // Transform matrix; stored in row-major order.
  std::vector<float> transform_;

  // Output buffer, if needed.
  std::vector<float> buffer_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_INTERLEAVED_CHANNEL_MIXER_H_
