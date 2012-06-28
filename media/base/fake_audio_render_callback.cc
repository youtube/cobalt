// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include "media/base/fake_audio_render_callback.h"

#include <cmath>

namespace media {

// Arbitrarily chosen prime value for the fake random number generator.
static const int kFakeRandomSeed = 9440671;

FakeAudioRenderCallback::FakeAudioRenderCallback(const AudioParameters& params)
    : half_fill_(false),
      fill_value_(1.0f),
      audio_parameters_(params) {
}

FakeAudioRenderCallback::~FakeAudioRenderCallback() {}

int FakeAudioRenderCallback::Render(const std::vector<float*>& audio_data,
                                    int number_of_frames,
                                    int audio_delay_milliseconds) {
  if (half_fill_)
    number_of_frames /= 2;
  for (size_t i = 0; i < audio_data.size(); ++i)
    std::fill(audio_data[i], audio_data[i] + number_of_frames, fill_value_);

  return number_of_frames;
}

void FakeAudioRenderCallback::NextFillValue() {
  // Use irrationality of PI to fake random numbers; square fill_value_ to keep
  // numbers between [0, 1) so range extension to [-1, 1) by 2 * x - 1 works.
  fill_value_ = 2.0f * (static_cast<int>(
      M_PI * fill_value_ * fill_value_ * kFakeRandomSeed) % kFakeRandomSeed) /
      static_cast<float>(kFakeRandomSeed) - 1.0f;
}

}  // namespace media
