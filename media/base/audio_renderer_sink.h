// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_RENDERER_SINK_H_
#define MEDIA_BASE_AUDIO_RENDERER_SINK_H_

#include <vector>
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "media/audio/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

// AudioRendererSink is an interface representing the end-point for
// rendered audio.  An implementation is expected to
// periodically call Render() on a callback object.

class AudioRendererSink
    : public base::RefCountedThreadSafe<media::AudioRendererSink> {
 public:
  class RenderCallback {
   public:
    // Fills entire buffer of length |number_of_frames| but returns actual
    // number of frames it got from its source (|number_of_frames| in case of
    // continuous stream). That actual number of frames is passed to host
    // together with PCM audio data and host is free to use or ignore it.
    // TODO(crogers): use base:Callback instead.
    virtual int Render(const std::vector<float*>& audio_data,
                       int number_of_frames,
                       int audio_delay_milliseconds) = 0;

    // Signals an error has occurred.
    virtual void OnRenderError() = 0;

   protected:
    virtual ~RenderCallback() {}
  };

  virtual ~AudioRendererSink() {}

  // Sets important information about the audio stream format.
  // It must be called before any of the other methods.
  virtual void Initialize(const AudioParameters& params,
                          RenderCallback* callback) = 0;

  // Starts audio playback.
  virtual void Start() = 0;

  // Stops audio playback.
  virtual void Stop() = 0;

  // Pauses playback.
  virtual void Pause(bool flush) = 0;

  // Resumes playback after calling Pause().
  virtual void Play() = 0;

  // Called to inform the sink of a change in playback rate. Override if
  // subclass needs the playback rate.
  virtual void SetPlaybackRate(float rate) {}

  // Sets the playback volume, with range [0.0, 1.0] inclusive.
  // Returns |true| on success.
  virtual bool SetVolume(double volume) = 0;

  // Gets the playback volume, with range [0.0, 1.0] inclusive.
  virtual void GetVolume(double* volume) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_RENDERER_SINK_H_
