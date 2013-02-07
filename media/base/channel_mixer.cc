// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_SQRT1_2.
#define _USE_MATH_DEFINES

#include "media/base/channel_mixer.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "media/base/audio_bus.h"
#include "media/base/vector_math.h"

namespace media {

// Default scale factor for mixing two channels together.  We use a different
// value for stereo -> mono and mono -> stereo mixes.
static const float kEqualPowerScale = static_cast<float>(M_SQRT1_2);

static int ValidateLayout(ChannelLayout layout) {
  CHECK_NE(layout, CHANNEL_LAYOUT_NONE);
  CHECK_NE(layout, CHANNEL_LAYOUT_MAX);

  // TODO(dalecurtis, crogers): We will eventually handle unsupported layouts by
  // simply copying the input channels to the output channels, similar to if the
  // user requests identical input and output layouts today.
  CHECK_NE(layout, CHANNEL_LAYOUT_UNSUPPORTED);

  // Verify there's at least one channel.  Should always be true here by virtue
  // of not being one of the invalid layouts, but lets double check to be sure.
  int channel_count = ChannelLayoutToChannelCount(layout);
  DCHECK_GT(channel_count, 0);

  // If we have more than one channel, verify a symmetric layout for sanity.
  // The unit test will verify all possible layouts, so this can be a DCHECK.
  // Symmetry allows simplifying the matrix building code by allowing us to
  // assume that if one channel of a pair exists, the other will too.
  if (channel_count > 1) {
    DCHECK((ChannelOrder(layout, LEFT) >= 0 &&
            ChannelOrder(layout, RIGHT) >= 0) ||
           (ChannelOrder(layout, SIDE_LEFT) >= 0 &&
            ChannelOrder(layout, SIDE_RIGHT) >= 0) ||
           (ChannelOrder(layout, BACK_LEFT) >= 0 &&
            ChannelOrder(layout, BACK_RIGHT) >= 0) ||
           (ChannelOrder(layout, LEFT_OF_CENTER) >= 0 &&
            ChannelOrder(layout, RIGHT_OF_CENTER) >= 0))
        << "Non-symmetric channel layout encountered.";
  } else {
    DCHECK_EQ(layout, CHANNEL_LAYOUT_MONO);
  }

  return channel_count;
}

ChannelMixer::ChannelMixer(ChannelLayout input, ChannelLayout output)
    : input_layout_(input),
      output_layout_(output),
      remapping_(false) {
  // Stereo down mix should never be the output layout.
  CHECK_NE(output_layout_, CHANNEL_LAYOUT_STEREO_DOWNMIX);

  int input_channels = ValidateLayout(input_layout_);
  int output_channels = ValidateLayout(output_layout_);

  // Size out the initial matrix.
  matrix_.reserve(output_channels);
  for (int output_ch = 0; output_ch < output_channels; ++output_ch)
    matrix_.push_back(std::vector<float>(input_channels, 0));

  // Route matching channels and figure out which ones aren't accounted for.
  for (Channels ch = LEFT; ch < CHANNELS_MAX;
       ch = static_cast<Channels>(ch + 1)) {
    int input_ch_index = ChannelOrder(input_layout_, ch);
    int output_ch_index = ChannelOrder(output_layout_, ch);

    if (input_ch_index < 0)
      continue;

    if (output_ch_index < 0) {
      unaccounted_inputs_.push_back(ch);
      continue;
    }

    DCHECK_LT(static_cast<size_t>(output_ch_index), matrix_.size());
    DCHECK_LT(static_cast<size_t>(input_ch_index),
              matrix_[output_ch_index].size());
    matrix_[output_ch_index][input_ch_index] = 1;
  }

  // If all input channels are accounted for, there's nothing left to do.
  if (unaccounted_inputs_.empty()) {
    // Since all output channels map directly to inputs we can optimize.
    remapping_ = true;
    return;
  }

  // Mix front LR into center.
  if (IsUnaccounted(LEFT)) {
    // When down mixing to mono from stereo, we need to be careful of full scale
    // stereo mixes.  Scaling by 1 / sqrt(2) here will likely lead to clipping
    // so we use 1 / 2 instead.
    float scale = (output == CHANNEL_LAYOUT_MONO && input_channels == 2) ?
        0.5 : kEqualPowerScale;
    Mix(LEFT, CENTER, scale);
    Mix(RIGHT, CENTER, scale);
  }

  // Mix center into front LR.
  if (IsUnaccounted(CENTER)) {
    // When up mixing from mono, just do a copy to front LR.
    float scale = (input == CHANNEL_LAYOUT_MONO) ? 1 : kEqualPowerScale;
    MixWithoutAccounting(CENTER, LEFT, scale);
    Mix(CENTER, RIGHT, scale);
  }

  // Mix back LR into: side LR || back center || front LR || front center.
  if (IsUnaccounted(BACK_LEFT)) {
    if (HasOutputChannel(SIDE_LEFT)) {
      // If we have side LR, mix back LR into side LR, but instead if the input
      // doesn't have side LR (but output does) copy back LR to side LR.
      float scale = HasInputChannel(SIDE_LEFT) ? kEqualPowerScale : 1;
      Mix(BACK_LEFT, SIDE_LEFT, scale);
      Mix(BACK_RIGHT, SIDE_RIGHT, scale);
    } else if (HasOutputChannel(BACK_CENTER)) {
      // Mix back LR into back center.
      Mix(BACK_LEFT, BACK_CENTER, kEqualPowerScale);
      Mix(BACK_RIGHT, BACK_CENTER, kEqualPowerScale);
    } else if (output > CHANNEL_LAYOUT_MONO) {
      // Mix back LR into front LR.
      Mix(BACK_LEFT, LEFT, kEqualPowerScale);
      Mix(BACK_RIGHT, RIGHT, kEqualPowerScale);
    } else {
      // Mix back LR into front center.
      Mix(BACK_LEFT, CENTER, kEqualPowerScale);
      Mix(BACK_RIGHT, CENTER, kEqualPowerScale);
    }
  }

  // Mix side LR into: back LR || back center || front LR || front center.
  if (IsUnaccounted(SIDE_LEFT)) {
    if (HasOutputChannel(BACK_LEFT)) {
      // If we have back LR, mix side LR into back LR, but instead if the input
      // doesn't have back LR (but output does) copy side LR to back LR.
      float scale = HasInputChannel(BACK_LEFT) ? kEqualPowerScale : 1;
      Mix(SIDE_LEFT, BACK_LEFT, scale);
      Mix(SIDE_RIGHT, BACK_RIGHT, scale);
    } else if (HasOutputChannel(BACK_CENTER)) {
      // Mix side LR into back center.
      Mix(SIDE_LEFT, BACK_CENTER, kEqualPowerScale);
      Mix(SIDE_RIGHT, BACK_CENTER, kEqualPowerScale);
    } else if (output > CHANNEL_LAYOUT_MONO) {
      // Mix side LR into front LR.
      Mix(SIDE_LEFT, LEFT, kEqualPowerScale);
      Mix(SIDE_RIGHT, RIGHT, kEqualPowerScale);
    } else {
      // Mix side LR into front center.
      Mix(SIDE_LEFT, CENTER, kEqualPowerScale);
      Mix(SIDE_RIGHT, CENTER, kEqualPowerScale);
    }
  }

  // Mix back center into: back LR || side LR || front LR || front center.
  if (IsUnaccounted(BACK_CENTER)) {
    if (HasOutputChannel(BACK_LEFT)) {
      // Mix back center into back LR.
      MixWithoutAccounting(BACK_CENTER, BACK_LEFT, kEqualPowerScale);
      Mix(BACK_CENTER, BACK_RIGHT, kEqualPowerScale);
    } else if (HasOutputChannel(SIDE_LEFT)) {
      // Mix back center into side LR.
      MixWithoutAccounting(BACK_CENTER, SIDE_LEFT, kEqualPowerScale);
      Mix(BACK_CENTER, SIDE_RIGHT, kEqualPowerScale);
    } else if (output > CHANNEL_LAYOUT_MONO) {
      // Mix back center into front LR.
      // TODO(dalecurtis): Not sure about these values?
      MixWithoutAccounting(BACK_CENTER, LEFT, kEqualPowerScale);
      Mix(BACK_CENTER, RIGHT, kEqualPowerScale);
    } else {
      // Mix back center into front center.
      // TODO(dalecurtis): Not sure about these values?
      Mix(BACK_CENTER, CENTER, kEqualPowerScale);
    }
  }

  // Mix LR of center into: front center || front LR.
  if (IsUnaccounted(LEFT_OF_CENTER)) {
    if (HasOutputChannel(LEFT)) {
      // Mix LR of center into front LR.
      Mix(LEFT_OF_CENTER, LEFT, kEqualPowerScale);
      Mix(RIGHT_OF_CENTER, RIGHT, kEqualPowerScale);
    } else {
      // Mix LR of center into front center.
      Mix(LEFT_OF_CENTER, CENTER, kEqualPowerScale);
      Mix(RIGHT_OF_CENTER, CENTER, kEqualPowerScale);
    }
  }

  // Mix LFE into: front LR || front center.
  if (IsUnaccounted(LFE)) {
    if (!HasOutputChannel(CENTER)) {
      // Mix LFE into front LR.
      MixWithoutAccounting(LFE, LEFT, kEqualPowerScale);
      Mix(LFE, RIGHT, kEqualPowerScale);
    } else {
      // Mix LFE into front center.
      Mix(LFE, CENTER, kEqualPowerScale);
    }
  }

  // All channels should now be accounted for.
  DCHECK(unaccounted_inputs_.empty());

  // See if the output |matrix_| is simply a remapping matrix.  If each input
  // channel maps to a single output channel we can simply remap.  Doing this
  // programmatically is less fragile than logic checks on channel mappings.
  for (int output_ch = 0; output_ch < output_channels; ++output_ch) {
    int input_mappings = 0;
    for (int input_ch = 0; input_ch < input_channels; ++input_ch) {
      // We can only remap if each row contains a single scale of 1.  I.e., each
      // output channel is mapped from a single unscaled input channel.
      if (matrix_[output_ch][input_ch] != 1 || ++input_mappings > 1)
        return;
    }
  }

  // If we've gotten here, |matrix_| is simply a remapping.
  remapping_ = true;
}

ChannelMixer::~ChannelMixer() {}

void ChannelMixer::Transform(const AudioBus* input, AudioBus* output) {
  CHECK_EQ(matrix_.size(), static_cast<size_t>(output->channels()));
  CHECK_EQ(matrix_[0].size(), static_cast<size_t>(input->channels()));
  CHECK_EQ(input->frames(), output->frames());

  // Zero initialize |output| so we're accumulating from zero.
  output->Zero();

  // If we're just remapping we can simply copy the correct input to output.
  if (remapping_) {
    for (int output_ch = 0; output_ch < output->channels(); ++output_ch) {
      for (int input_ch = 0; input_ch < input->channels(); ++input_ch) {
        float scale = matrix_[output_ch][input_ch];
        if (scale > 0) {
          DCHECK_EQ(scale, 1.0f);
          memcpy(output->channel(output_ch), input->channel(input_ch),
                 sizeof(*output->channel(output_ch)) * output->frames());
          break;
        }
      }
    }
    return;
  }

  for (int output_ch = 0; output_ch < output->channels(); ++output_ch) {
    for (int input_ch = 0; input_ch < input->channels(); ++input_ch) {
      float scale = matrix_[output_ch][input_ch];
      // Scale should always be positive.  Don't bother scaling by zero.
      DCHECK_GE(scale, 0);
      if (scale > 0) {
        vector_math::FMAC(input->channel(input_ch), scale, output->frames(),
                          output->channel(output_ch));
      }
    }
  }
}

void ChannelMixer::AccountFor(Channels ch) {
  unaccounted_inputs_.erase(std::find(
      unaccounted_inputs_.begin(), unaccounted_inputs_.end(), ch));
}

bool ChannelMixer::IsUnaccounted(Channels ch) {
  return std::find(unaccounted_inputs_.begin(), unaccounted_inputs_.end(),
                   ch) != unaccounted_inputs_.end();
}

bool ChannelMixer::HasInputChannel(Channels ch) {
  return ChannelOrder(input_layout_, ch) >= 0;
}

bool ChannelMixer::HasOutputChannel(Channels ch) {
  return ChannelOrder(output_layout_, ch) >= 0;
}

void ChannelMixer::Mix(Channels input_ch, Channels output_ch, float scale) {
  MixWithoutAccounting(input_ch, output_ch, scale);
  AccountFor(input_ch);
}

void ChannelMixer::MixWithoutAccounting(Channels input_ch, Channels output_ch,
                                        float scale) {
  int input_ch_index = ChannelOrder(input_layout_, input_ch);
  int output_ch_index = ChannelOrder(output_layout_, output_ch);

  DCHECK(IsUnaccounted(input_ch));
  DCHECK_GE(input_ch_index, 0);
  DCHECK_GE(output_ch_index, 0);

  DCHECK_EQ(matrix_[output_ch_index][input_ch_index], 0);
  matrix_[output_ch_index][input_ch_index] = scale;
}

}  // namespace media
