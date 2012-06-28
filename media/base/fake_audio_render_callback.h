// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FAKE_AUDIO_RENDER_CALLBACK_H_
#define MEDIA_BASE_FAKE_AUDIO_RENDER_CALLBACK_H_

#include <vector>

#include "base/time.h"
#include "media/base/audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class FakeAudioRenderCallback : public AudioRendererSink::RenderCallback {
 public:
  // Initializes |fill_value_| to a random value seeded by current time.
  explicit FakeAudioRenderCallback(const AudioParameters& params);
  virtual ~FakeAudioRenderCallback();

  // Renders |fill_value_| into the provided audio data buffer.  If |half_fill_|
  // is set, will only fill half the buffer.
  int Render(const std::vector<float*>& audio_data, int number_of_frames,
             int audio_delay_milliseconds) OVERRIDE;
  MOCK_METHOD0(OnRenderError, void());

  // Toggles only filling half the requested amount during Render().
  void set_half_fill(bool half_fill) { half_fill_ = half_fill; }

  float fill_value() { return fill_value_; }

  // Generates a fill value between [-1, 1).  NextFillValue() is deterministic
  // based on fill_value_.  Which means it will generate the same sequence of
  // fill values every time unless |fill_value_| is set using set_fill_value().
  void NextFillValue();

 private:
  bool half_fill_;
  float fill_value_;
  AudioParameters audio_parameters_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioRenderCallback);
};

}  // namespace media

#endif  // MEDIA_BASE_FAKE_AUDIO_RENDER_CALLBACK_H_
