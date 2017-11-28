// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_INPUT_STREAM_IMPL_H_
#define MEDIA_AUDIO_AUDIO_INPUT_STREAM_IMPL_H_

#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "media/audio/audio_io.h"

namespace media {

// AudioInputStreamImpl implements platform-independent parts of the
// AudioInputStream interface. Each platform dependent implementation
// should derive from this class.
// TODO(henrika): we can probably break out more parts from our current
// AudioInputStream implementation and move out to this class.
class MEDIA_EXPORT AudioInputStreamImpl : public AudioInputStream {
 public:
  AudioInputStreamImpl();
  virtual ~AudioInputStreamImpl();

  // Sets the automatic gain control (AGC) to on or off. When AGC is enabled,
  // the microphone volume is queried periodically and the volume level is
  // provided in each AudioInputCallback::OnData() callback and fed to the
  // render-side AGC.
  virtual void SetAutomaticGainControl(bool enabled) override;

  // Gets the current automatic gain control state.
  virtual bool GetAutomaticGainControl() override;

 protected:
  // Stores a new volume level by asking the audio hardware.
  // This method only has an effect if AGC is enabled.
  void UpdateAgcVolume();

  // Gets the latest stored volume level if AGC is enabled and if
  // more than one second has passed since the volume was updated the last time.
  void QueryAgcVolume(double* normalized_volume);

 private:
  // Takes a volume sample and stores it in |normalized_volume_|.
  void GetNormalizedVolume();

  // True when automatic gain control is enabled, false otherwise.
  // Guarded by |lock_|.
  bool agc_is_enabled_;

  // Stores the maximum volume which is used for normalization to a volume
  // range of [0.0, 1.0].
  double max_volume_;

  // Contains last result of internal call to GetVolume(). We save resources
  // but not querying the capture volume for each callback. Guarded by |lock_|.
  // The range is normalized to [0.0, 1.0].
  double normalized_volume_;

  // Protects |agc_is_enabled_| and |volume_| .
  base::Lock lock_;

  // Keeps track of the last time the microphone volume level was queried.
  base::Time last_volume_update_time_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputStreamImpl);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_INPUT_STREAM_IMPL_H_
