// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_FAKE_AUDIO_RENDERER_SINK_H_
#define COBALT_MEDIA_BASE_FAKE_AUDIO_RENDERER_SINK_H_

#include <string>

#include "base/basictypes.h"
#include "cobalt/media/base/audio_parameters.h"
#include "cobalt/media/base/audio_renderer_sink.h"
#include "cobalt/media/base/output_device_info.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

class FakeAudioRendererSink : public AudioRendererSink {
 public:
  enum State {
    kUninitialized,
    kInitialized,
    kStarted,
    kPaused,
    kPlaying,
    kStopped
  };

  FakeAudioRendererSink();

  explicit FakeAudioRendererSink(const AudioParameters& hardware_params);

  void Initialize(const AudioParameters& params,
                  RenderCallback* callback) OVERRIDE;
  void Start() OVERRIDE;
  void Stop() OVERRIDE;
  void Pause() OVERRIDE;
  void Play() OVERRIDE;
  bool SetVolume(double volume) OVERRIDE;
  OutputDeviceInfo GetOutputDeviceInfo() OVERRIDE;
  bool CurrentThreadIsRenderingThread() OVERRIDE;

  // Attempts to call Render() on the callback provided to
  // Initialize() with |dest| and |frames_delayed|.
  // Returns true and sets |frames_written| to the return value of the
  // Render() call.
  // Returns false if this object is in a state where calling Render()
  // should not occur. (i.e., in the kPaused or kStopped state.) The
  // value of |frames_written| is undefined if false is returned.
  bool Render(AudioBus* dest, uint32_t frames_delayed, int* frames_written);
  void OnRenderError();

  State state() const { return state_; }

 protected:
  ~FakeAudioRendererSink() OVERRIDE;

 private:
  void ChangeState(State new_state);

  State state_;
  RenderCallback* callback_;
  OutputDeviceInfo output_device_info_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioRendererSink);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_FAKE_AUDIO_RENDERER_SINK_H_
