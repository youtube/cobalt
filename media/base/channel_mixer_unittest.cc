// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_SQRT1_2.
#define _USE_MATH_DEFINES

#include <cmath>

#include "base/stringprintf.h"
#include "media/base/audio_bus.h"
#include "media/base/channel_mixer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Number of frames to test with.
enum { kFrames = 16 };

// Test all possible layout conversions can be constructed and mixed.
TEST(ChannelMixerTest, ConstructAllPossibleLayouts) {
  for (ChannelLayout input_layout = CHANNEL_LAYOUT_MONO;
       input_layout < CHANNEL_LAYOUT_MAX;
       input_layout = static_cast<ChannelLayout>(input_layout + 1)) {
    for (ChannelLayout output_layout = CHANNEL_LAYOUT_MONO;
         output_layout < CHANNEL_LAYOUT_STEREO_DOWNMIX;
         output_layout = static_cast<ChannelLayout>(output_layout + 1)) {
      SCOPED_TRACE(base::StringPrintf(
          "Input Layout: %d, Output Layout: %d", input_layout, output_layout));
      ChannelMixer mixer(input_layout, output_layout);
      scoped_ptr<AudioBus> input_bus = AudioBus::Create(
          ChannelLayoutToChannelCount(input_layout), kFrames);
      scoped_ptr<AudioBus> output_bus = AudioBus::Create(
          ChannelLayoutToChannelCount(output_layout), kFrames);
      for (int ch = 0; ch < input_bus->channels(); ++ch)
        std::fill(input_bus->channel(ch), input_bus->channel(ch) + kFrames, 1);

      mixer.Transform(input_bus.get(), output_bus.get());
    }
  }
}

struct ChannelMixerTestData {
  ChannelMixerTestData(ChannelLayout input_layout, ChannelLayout output_layout,
                       float* channel_values, int num_channel_values,
                       float scale)
      : input_layout(input_layout),
        output_layout(output_layout),
        channel_values(channel_values),
        num_channel_values(num_channel_values),
        scale(scale) {
  }

  std::string DebugString() const {
    return base::StringPrintf(
        "Input Layout: %d, Output Layout %d, Scale: %f", input_layout,
        output_layout, scale);
  }

  ChannelLayout input_layout;
  ChannelLayout output_layout;
  float* channel_values;
  int num_channel_values;
  float scale;
};

std::ostream& operator<<(std::ostream& os, const ChannelMixerTestData& data) {
  return os << data.DebugString();
}

class ChannelMixerTest : public testing::TestWithParam<ChannelMixerTestData> {};

// Verify channels are mixed and scaled correctly.  The test only works if all
// output channels have the same value.
TEST_P(ChannelMixerTest, Mixing) {
  ChannelLayout input_layout = GetParam().input_layout;
  ChannelLayout output_layout = GetParam().output_layout;

  ChannelMixer mixer(input_layout, output_layout);
  scoped_ptr<AudioBus> input_bus = AudioBus::Create(
      ChannelLayoutToChannelCount(input_layout), kFrames);
  scoped_ptr<AudioBus> output_bus = AudioBus::Create(
      ChannelLayoutToChannelCount(output_layout), kFrames);

  const float* channel_values = GetParam().channel_values;
  ASSERT_EQ(input_bus->channels(), GetParam().num_channel_values);

  float expected_value = 0;
  float scale = GetParam().scale;
  for (int ch = 0; ch < input_bus->channels(); ++ch) {
    std::fill(input_bus->channel(ch), input_bus->channel(ch) + kFrames,
              channel_values[ch]);
    expected_value += channel_values[ch] * scale;
  }

  mixer.Transform(input_bus.get(), output_bus.get());

  for (int ch = 0; ch < output_bus->channels(); ++ch) {
    for (int frame = 0; frame < output_bus->frames(); ++frame) {
      ASSERT_FLOAT_EQ(output_bus->channel(ch)[frame], expected_value);
    }
  }
}

static float kStereoToMonoValues[] = { 0.5f, 0.75f };
static float kMonoToStereoValues[] = { 0.5f };
// Zero the center channel since it will be mixed at scale 1 vs M_SQRT1_2.
static float kFiveOneToMonoValues[] = { 0.1f, 0.2f, 0.0f, 0.4f, 0.5f, 0.6f };

// Run through basic sanity tests for some common conversions.
INSTANTIATE_TEST_CASE_P(ChannelMixerTest, ChannelMixerTest, testing::Values(
    ChannelMixerTestData(CHANNEL_LAYOUT_STEREO, CHANNEL_LAYOUT_MONO,
                         kStereoToMonoValues, arraysize(kStereoToMonoValues),
                         0.5f),
    ChannelMixerTestData(CHANNEL_LAYOUT_MONO, CHANNEL_LAYOUT_STEREO,
                         kMonoToStereoValues, arraysize(kMonoToStereoValues),
                         1.0f),
    ChannelMixerTestData(CHANNEL_LAYOUT_5_1, CHANNEL_LAYOUT_MONO,
                         kFiveOneToMonoValues, arraysize(kFiveOneToMonoValues),
                         static_cast<float>(M_SQRT1_2))
));

}  // namespace media
