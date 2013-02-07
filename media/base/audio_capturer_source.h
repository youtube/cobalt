// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_CAPTURER_SOURCE_H_
#define MEDIA_BASE_AUDIO_CAPTURER_SOURCE_H_

#include <vector>
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/media_export.h"

namespace media {

// AudioCapturerSource is an interface representing the source for
// captured audio.  An implementation will periodically call Capture() on a
// callback object.
class AudioCapturerSource
    : public base::RefCountedThreadSafe<media::AudioCapturerSource> {
 public:
  class CaptureCallback {
   public:
    // Callback to deliver the captured data from the OS.
    virtual void Capture(AudioBus* audio_source,
                         int audio_delay_milliseconds,
                         double volume) = 0;

    // Signals an error has occurred.
    virtual void OnCaptureError() = 0;

   protected:
    virtual ~CaptureCallback() {}
  };

  class CaptureEventHandler {
   public:
    // Notification to the client that the device with the specific |device_id|
    // has been started.
    virtual void OnDeviceStarted(const std::string& device_id) = 0;

    // Notification to the client that the device has been stopped.
    virtual void OnDeviceStopped() = 0;

   protected:
    virtual ~CaptureEventHandler() {}
  };

  // Sets information about the audio stream format and the device
  // to be used. It must be called before any of the other methods.
  // TODO(xians): Add |device_id| to this Initialize() function.
  virtual void Initialize(const AudioParameters& params,
                          CaptureCallback* callback,
                          CaptureEventHandler* event_handler) = 0;

  // Starts the audio recording.
  virtual void Start() = 0;

  // Stops the audio recording. This API is synchronous, and no more data
  // callback will be passed to the client after it is being called.
  virtual void Stop() = 0;

  // Sets the capture volume, with range [0.0, 1.0] inclusive.
  virtual void SetVolume(double volume) = 0;

  // Specifies the |session_id| to query which device to use.
  // TODO(xians): Change the interface to SetDevice(const std::string&).
  virtual void SetDevice(int session_id) = 0;

  // Enables or disables the WebRtc AGC control.
  virtual void SetAutomaticGainControl(bool enable) = 0;

 protected:
  friend class base::RefCountedThreadSafe<AudioCapturerSource>;
  virtual ~AudioCapturerSource() {}
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_CAPTURER_SOURCE_H_
