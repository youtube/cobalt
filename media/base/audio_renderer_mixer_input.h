// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_RENDERER_MIXER_INPUT_H_
#define MEDIA_BASE_AUDIO_RENDERER_MIXER_INPUT_H_

#include <vector>

#include "base/callback.h"
#include "media/base/audio_renderer_sink.h"

namespace media {

class AudioRendererMixer;

class MEDIA_EXPORT AudioRendererMixerInput
    : NON_EXPORTED_BASE(public AudioRendererSink) {
 public:
  typedef base::Callback<AudioRendererMixer*(
      const AudioParameters& params)> GetMixerCB;
  typedef base::Callback<void(const AudioParameters& params)> RemoveMixerCB;

  AudioRendererMixerInput(
      const GetMixerCB& get_mixer_cb, const RemoveMixerCB& remove_mixer_cb);

  AudioRendererSink::RenderCallback* callback() { return callback_; }
  bool playing() { return playing_; }

  // AudioRendererSink implementation.
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Play() OVERRIDE;
  virtual void Pause(bool flush) OVERRIDE;
  virtual bool SetVolume(double volume) OVERRIDE;
  virtual void Initialize(const AudioParameters& params,
                          AudioRendererSink::RenderCallback* renderer) OVERRIDE;

  void GetVolume(double* volume);

 protected:
  virtual ~AudioRendererMixerInput();

 private:
  bool playing_;
  bool initialized_;
  double volume_;

  // Callbacks provided during construction which allow AudioRendererMixerInput
  // to retrieve a mixer during Initialize() and notify when it's done with it.
  GetMixerCB get_mixer_cb_;
  RemoveMixerCB remove_mixer_cb_;

  // AudioParameters received during Initialize().
  AudioParameters params_;

  // AudioRendererMixer provided through |get_mixer_cb_| during Initialize(),
  // guaranteed to live (at least) until |remove_mixer_cb_| is called.
  AudioRendererMixer* mixer_;

  // Source of audio data which is provided to the mixer.
  AudioRendererSink::RenderCallback* callback_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererMixerInput);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_RENDERER_MIXER_INPUT_H_
