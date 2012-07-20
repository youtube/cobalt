// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include "media/base/fake_audio_render_callback.h"

#include <cmath>

namespace media {

FakeAudioRenderCallback::FakeAudioRenderCallback(double step)
    : half_fill_(false),
      step_(step) {
  reset();
}

FakeAudioRenderCallback::~FakeAudioRenderCallback() {}

int FakeAudioRenderCallback::Render(const std::vector<float*>& audio_data,
                                    int number_of_frames,
                                    int audio_delay_milliseconds) {
  if (half_fill_)
    number_of_frames /= 2;

  // Fill first channel with a sine wave.
  for (int i = 0; i < number_of_frames; ++i)
    audio_data[0][i] = sin(2 * M_PI * (x_ + step_ * i));
  x_ += number_of_frames * step_;

  // Copy first channel into the rest of the channels.
  for (size_t i = 1; i < audio_data.size(); ++i)
    memcpy(audio_data[i], audio_data[0],
           number_of_frames * sizeof(*audio_data[0]));

  return number_of_frames;
}

}  // namespace media
