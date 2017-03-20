// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_AUDIO_CAPTURER_SOURCE_H_
#define COBALT_MEDIA_BASE_AUDIO_CAPTURER_SOURCE_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/media/base/audio_bus.h"
#include "cobalt/media/base/audio_parameters.h"
#include "cobalt/media/base/media_export.h"

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
    // TODO(chcunningham): Update delay argument to use frames instead of
    // milliseconds to prevent loss of precision. See http://crbug.com/587291.
    virtual void Capture(const AudioBus* audio_source,
                         int audio_delay_milliseconds, double volume,
                         bool key_pressed) = 0;

    // Signals an error has occurred.
    virtual void OnCaptureError(const std::string& message) = 0;

   protected:
    virtual ~CaptureCallback() {}
  };

  // Sets information about the audio stream format and the device
  // to be used. It must be called before any of the other methods.
  // The |session_id| is used by the browser to identify which input device to
  // be used. For clients who do not care about device permission and device
  // selection, pass |session_id| using
  // AudioInputDeviceManager::kFakeOpenSessionId.
  virtual void Initialize(const AudioParameters& params,
                          CaptureCallback* callback, int session_id) = 0;

  // Starts the audio recording.
  virtual void Start() = 0;

  // Stops the audio recording. This API is synchronous, and no more data
  // callback will be passed to the client after it is being called.
  virtual void Stop() = 0;

  // Sets the capture volume, with range [0.0, 1.0] inclusive.
  virtual void SetVolume(double volume) = 0;

  // Enables or disables the WebRtc AGC control.
  virtual void SetAutomaticGainControl(bool enable) = 0;

 protected:
  friend class base::RefCountedThreadSafe<AudioCapturerSource>;
  virtual ~AudioCapturerSource() {}
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_AUDIO_CAPTURER_SOURCE_H_
