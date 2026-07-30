// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_PROCESSING_H_
#define MEDIA_BASE_AUDIO_PROCESSING_H_

#include <string>

#include "build/build_config.h"
#include "media/base/media_export.h"
#include "media/media_buildflags.h"

namespace media {

// This struct specifies software audio processing effects to be applied by
// Chrome to mic capture audio. If system / hardware effects replace effects in
// this struct, then the corresponding parameters in the struct should be
// disabled.
struct MEDIA_EXPORT AudioProcessingSettings {
  bool echo_cancellation = true;
  bool noise_suppression = true;
  bool automatic_gain_control = true;
  // Multi-channel is not an individual audio effect, but determines whether the
  // processing algorithms should preserve multi-channel input audio.
  bool multi_channel_capture_processing = true;
  // If true, a system loopback stream will be used as the echo cancellation
  // reference signal.
  bool use_loopback_aec_reference = false;
#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  // If true, voice isolation will be applied.
  bool voice_isolation = false;
#endif

  bool operator==(const AudioProcessingSettings& b) const {
    bool equal = echo_cancellation == b.echo_cancellation &&
                 noise_suppression == b.noise_suppression &&
                 automatic_gain_control == b.automatic_gain_control &&
                 multi_channel_capture_processing ==
                     b.multi_channel_capture_processing &&
                 use_loopback_aec_reference == b.use_loopback_aec_reference;
#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
    equal = equal && voice_isolation == b.voice_isolation;
#endif
    return equal;
  }

  bool NeedWebrtcAudioProcessing() const {
    // TODO(crbug.com/40205004): Legacy iOS-specific behavior;
    // reconsider.
#if !BUILDFLAG(IS_IOS)
    if (echo_cancellation || automatic_gain_control) {
      return true;
    }
#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
    if (voice_isolation) {
      return true;
    }
#endif
#endif

    return noise_suppression;
  }

  // Stringifies the settings for human-readable logging.
  std::string ToString() const;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_PROCESSING_H_
