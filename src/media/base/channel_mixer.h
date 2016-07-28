// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CHANNEL_MIXER_H_
#define MEDIA_BASE_CHANNEL_MIXER_H_

#include <vector>

#include "base/basictypes.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"

namespace media {

class AudioBus;

// ChannelMixer is for converting audio between channel layouts.  The conversion
// matrix is built upon construction and used during each Transform() call.  The
// algorithm works by generating a conversion matrix mapping each output channel
// to list of input channels.  The transform renders all of the output channels,
// with each output channel rendered according to a weighted sum of the relevant
// input channels as defined in the matrix.
class MEDIA_EXPORT ChannelMixer {
 public:
  ChannelMixer(ChannelLayout input, ChannelLayout output);
  ~ChannelMixer();

  // Transforms all channels from |input| into |output| channels.
  void Transform(const AudioBus* input, AudioBus* output);

 private:
  // Constructor helper methods for managing unaccounted input channels.
  void AccountFor(Channels ch);
  bool IsUnaccounted(Channels ch);

  // Helper methods for checking if |ch| exists in either |input_layout_| or
  // |output_layout_| respectively.
  bool HasInputChannel(Channels ch);
  bool HasOutputChannel(Channels ch);

  // Constructor helper methods for updating |matrix_| with the proper value for
  // mixing |input_ch| into |output_ch|.  MixWithoutAccounting() does not remove
  // the channel from |unaccounted_inputs_|.
  void Mix(Channels input_ch, Channels output_ch, float scale);
  void MixWithoutAccounting(Channels input_ch, Channels output_ch, float scale);

  // Input and output channel layout provided during construction.
  ChannelLayout input_layout_;
  ChannelLayout output_layout_;

  // Helper variable for tracking which inputs are currently unaccounted, should
  // be empty after construction completes.
  std::vector<Channels> unaccounted_inputs_;

  // 2D matrix of output channels to input channels.
  std::vector< std::vector<float> > matrix_;

  // Optimization case for when we can simply remap the input channels to output
  // channels and don't need to do a multiply-accumulate loop over |matrix_|.
  bool remapping_;

  DISALLOW_COPY_AND_ASSIGN(ChannelMixer);
};

}  // namespace media

#endif  // MEDIA_BASE_CHANNEL_MIXER_H_
