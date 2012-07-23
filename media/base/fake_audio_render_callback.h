// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FAKE_AUDIO_RENDER_CALLBACK_H_
#define MEDIA_BASE_FAKE_AUDIO_RENDER_CALLBACK_H_

#include <vector>

#include "media/base/audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// Fake RenderCallback which will fill each request with a sine wave.  Sine
// state is kept across callbacks.  State can be reset to default via reset().
class FakeAudioRenderCallback : public AudioRendererSink::RenderCallback {
 public:
  // The function used to fulfill Render() is f(x) = sin(2 * PI * x * |step|),
  // where x = [|number_of_frames| * m, |number_of_frames| * (m + 1)] and m =
  // the number of Render() calls fulfilled thus far.
  explicit FakeAudioRenderCallback(double step);
  virtual ~FakeAudioRenderCallback();

  // Renders a sine wave into the provided audio data buffer.  If |half_fill_|
  // is set, will only fill half the buffer.
  int Render(const std::vector<float*>& audio_data, int number_of_frames,
             int audio_delay_milliseconds) OVERRIDE;
  MOCK_METHOD0(OnRenderError, void());

  // Toggles only filling half the requested amount during Render().
  void set_half_fill(bool half_fill) { half_fill_ = half_fill; }

  // Reset the sine state to initial value.
  void reset() { x_ = 0; }

 private:
  bool half_fill_;
  double x_;
  double step_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioRenderCallback);
};

}  // namespace media

#endif  // MEDIA_BASE_FAKE_AUDIO_RENDER_CALLBACK_H_
