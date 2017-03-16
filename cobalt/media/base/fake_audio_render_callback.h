// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_FAKE_AUDIO_RENDER_CALLBACK_H_
#define COBALT_MEDIA_BASE_FAKE_AUDIO_RENDER_CALLBACK_H_

#include "base/basictypes.h"
#include "cobalt/media/base/audio_converter.h"
#include "cobalt/media/base/audio_renderer_sink.h"
#include "starboard/types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace media {

// Fake RenderCallback which will fill each request with a sine wave.  Sine
// state is kept across callbacks.  State can be reset to default via reset().
// Also provide an interface to AudioTransformInput.
class FakeAudioRenderCallback : public AudioRendererSink::RenderCallback,
                                public AudioConverter::InputCallback {
 public:
  // The function used to fulfill Render() is f(x) = sin(2 * PI * x * |step|),
  // where x = [|number_of_frames| * m, |number_of_frames| * (m + 1)] and m =
  // the number of Render() calls fulfilled thus far.
  explicit FakeAudioRenderCallback(double step);
  ~FakeAudioRenderCallback() OVERRIDE;

  // Renders a sine wave into the provided audio data buffer.  If |half_fill_|
  // is set, will only fill half the buffer.
  int Render(AudioBus* audio_bus, uint32_t frames_delayed,
             uint32_t frames_skipped) OVERRIDE;
  MOCK_METHOD0(OnRenderError, void());

  // AudioTransform::ProvideAudioTransformInput implementation.
  double ProvideInput(AudioBus* audio_bus, uint32_t frames_delayed) OVERRIDE;

  // Toggles only filling half the requested amount during Render().
  void set_half_fill(bool half_fill) { half_fill_ = half_fill; }

  // Reset the sine state to initial value.
  void reset() { x_ = 0; }

  // Returns the last |frames_delayed| provided to Render() or -1 if
  // no Render() call occurred.
  int last_frames_delayed() const { return last_frames_delayed_; }

  // Set volume information used by ProvideAudioTransformInput().
  void set_volume(double volume) { volume_ = volume; }

  int last_channel_count() const { return last_channel_count_; }

 private:
  int RenderInternal(AudioBus* audio_bus, uint32_t frames_delayed,
                     double volume);

  bool half_fill_;
  double x_;
  double step_;
  int last_frames_delayed_;
  int last_channel_count_;
  double volume_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioRenderCallback);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_FAKE_AUDIO_RENDER_CALLBACK_H_
