// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_RENDERER_MIXER_INPUT_H_
#define MEDIA_BASE_AUDIO_RENDERER_MIXER_INPUT_H_

#include <vector>

#include "media/base/audio_renderer_sink.h"

namespace media {

class AudioRendererMixer;

class MEDIA_EXPORT AudioRendererMixerInput
    : NON_EXPORTED_BASE(public AudioRendererSink) {
 public:
  explicit AudioRendererMixerInput(
      const scoped_refptr<AudioRendererMixer>& mixer);

  // Each input should manage its own data buffer.  The mixer will call this
  // method when it needs a buffer for rendering.
  const std::vector<float*>& audio_data() { return audio_data_; }
  AudioRendererSink::RenderCallback* callback() { return callback_; }
  bool playing() { return playing_; }

  // AudioRendererSink implementation.
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Play() OVERRIDE;
  virtual void Pause(bool flush) OVERRIDE;
  virtual bool SetVolume(double volume) OVERRIDE;
  virtual void GetVolume(double* volume) OVERRIDE;
  virtual void Initialize(const AudioParameters& params,
                          AudioRendererSink::RenderCallback* renderer) OVERRIDE;

 protected:
  virtual ~AudioRendererMixerInput();

 private:
  bool playing_;
  bool initialized_;
  double volume_;

  // AudioRendererMixer is reference counted by all its AudioRendererMixerInputs
  // and is destroyed when all AudioRendererMixerInputs have called RemoveMixer.
  scoped_refptr<AudioRendererMixer> mixer_;

  // Source of audio data which is provided to the mixer.
  AudioRendererSink::RenderCallback* callback_;

  // Vector for rendering audio data which will be used by the mixer.
  std::vector<float*> audio_data_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererMixerInput);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_RENDERER_MIXER_INPUT_H_
