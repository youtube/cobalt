// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_PROCESSING_H_
#define MEDIA_BASE_AUDIO_PROCESSING_H_

#include <string>

#include "build/build_config.h"
#include "media/base/media_export.h"

namespace media {

// This struct specifies software audio processing effects to be applied by
// Chrome to mic capture audio. If system / hardware effects replace effects in
// this struct, then the corresponding parameters in the struct should be
// disabled.
struct MEDIA_EXPORT AudioProcessingSettings {
  bool echo_cancellation = true;
  bool noise_suppression = true;
  // Keytap removal, sometimes called "experimental noise suppression".
  bool transient_noise_suppression = true;
  bool automatic_gain_control = true;
  // TODO(bugs.webrtc.org/7494): Remove since it is unused. On non-Chromecast
  // platforms, it has no effect.
  bool experimental_automatic_gain_control = true;
  bool high_pass_filter = true;
  // Multi-channel is not an individual audio effect, but determines whether the
  // processing algorithms should preserve multi-channel input audio.
  bool multi_channel_capture_processing = true;
  bool stereo_mirroring = false;

  // TODO(https://crbug.com/1215061): Deprecate this setting.
  // This flag preserves the behavior of the to-be-deprecated flag / constraint
  // |AudioProcessingProperties::goog_experimental_echo_cancellation|: It has no
  // effect on what effects are enabled, but for legacy reasons, it forces APM
  // to be created and used.
  bool force_apm_creation =
#if defined(OS_ANDROID)
      false;
#else
      true;
#endif

  bool operator==(const AudioProcessingSettings& b) const {
    return echo_cancellation == b.echo_cancellation &&
           noise_suppression == b.noise_suppression &&
           transient_noise_suppression == b.transient_noise_suppression &&
           automatic_gain_control == b.automatic_gain_control &&
           experimental_automatic_gain_control ==
               b.experimental_automatic_gain_control &&
           high_pass_filter == b.high_pass_filter &&
           multi_channel_capture_processing ==
               b.multi_channel_capture_processing &&
           stereo_mirroring == b.stereo_mirroring &&
           force_apm_creation == b.force_apm_creation;
  }

  // Stringifies the settings for human-readable logging.
  std::string ToString() const;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_PROCESSING_H_
