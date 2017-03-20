// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_AUDIO_RENDERER_SINK_H_
#define COBALT_MEDIA_BASE_AUDIO_RENDERER_SINK_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "cobalt/media/base/audio_bus.h"
#include "cobalt/media/base/audio_parameters.h"
#include "cobalt/media/base/output_device_info.h"
#include "url/origin.h"

namespace media {

// AudioRendererSink is an interface representing the end-point for
// rendered audio.  An implementation is expected to
// periodically call Render() on a callback object.

class AudioRendererSink
    : public base::RefCountedThreadSafe<media::AudioRendererSink> {
 public:
  class RenderCallback {
   public:
    // Attempts to completely fill all channels of |dest|, returns actual
    // number of frames filled. |frames_skipped| contains the number of frames
    // the consumer has skipped, if any.
    // TODO(jameswest): Change to use the same signature as
    // AudioOutputStream::AudioSourceCallback::OnMoreData.
    virtual int Render(AudioBus* dest, uint32_t frames_delayed,
                       uint32_t frames_skipped) = 0;

    // Signals an error has occurred.
    virtual void OnRenderError() = 0;

   protected:
    virtual ~RenderCallback() {}
  };

  // Sets important information about the audio stream format.
  // It must be called before any of the other methods.
  virtual void Initialize(const AudioParameters& params,
                          RenderCallback* callback) = 0;

  // Starts audio playback.
  virtual void Start() = 0;

  // Stops audio playback and performs cleanup. It must be called before
  // destruction.
  virtual void Stop() = 0;

  // Pauses playback.
  virtual void Pause() = 0;

  // Resumes playback after calling Pause().
  virtual void Play() = 0;

  // Sets the playback volume, with range [0.0, 1.0] inclusive.
  // Returns |true| on success.
  virtual bool SetVolume(double volume) = 0;

  // Returns current output device information. If the information is not
  // available yet, this method may block until it becomes available.
  // If the sink is not associated with any output device, |device_status| of
  // OutputDeviceInfo should be set to OUTPUT_DEVICE_STATUS_ERROR_INTERNAL.
  // Must never be called on the IO thread.
  virtual OutputDeviceInfo GetOutputDeviceInfo() = 0;

  // If DCHECKs are enabled, this function returns true if called on rendering
  // thread, otherwise false. With DCHECKs disabled, it returns true. Thus, it
  // is intended to be used for DCHECKing.
  virtual bool CurrentThreadIsRenderingThread() = 0;

 protected:
  friend class base::RefCountedThreadSafe<AudioRendererSink>;
  virtual ~AudioRendererSink() {}
};

// Same as AudioRendererSink except that Initialize() and Start() can be called
// again after Stop().
// TODO(sandersd): Fold back into AudioRendererSink once all subclasses support
// this.

class RestartableAudioRendererSink : public AudioRendererSink {
 protected:
  ~RestartableAudioRendererSink() override {}
};

class SwitchableAudioRendererSink : public RestartableAudioRendererSink {
 public:
  // Attempts to switch the audio output device associated with a sink.
  // Once the attempt is finished, |callback| is invoked with the
  // result of the operation passed as a parameter. The result is a value from
  // the media::OutputDeviceStatus enum.
  // There is no guarantee about the thread where |callback| will be invoked.
  virtual void SwitchOutputDevice(const std::string& device_id,
                                  const url::Origin& security_origin,
                                  const OutputDeviceStatusCB& callback) = 0;

 protected:
  ~SwitchableAudioRendererSink() override {}
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_AUDIO_RENDERER_SINK_H_
