// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_

#include "build/build_config.h"
#include "media/base/audio_processing.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace blink {

// Simple struct with audio-processing properties.
struct PLATFORM_EXPORT AudioProcessingProperties {
  enum class EchoCancellationType {
    // Echo cancellation disabled.
    kEchoCancellationDisabled,
    // The WebRTC-provided AEC3 echo canceller.
    kEchoCancellationAec3,
    // System echo canceller, for example an OS-provided or hardware echo
    // canceller.
    kEchoCancellationSystem
  };

  // Disables properties that are enabled by default.
  void DisableDefaultProperties();

  // Returns whether echo cancellation is enabled.
  bool EchoCancellationEnabled() const;

  // Returns whether WebRTC-provided echo cancellation is enabled.
  bool EchoCancellationIsWebRtcProvided() const;

  bool HasSameReconfigurableSettings(
      const AudioProcessingProperties& other) const;

  bool HasSameNonReconfigurableSettings(
      const AudioProcessingProperties& other) const;

  // Converts this struct to an equivalent media::AudioProcessingSettings.
  media::AudioProcessingSettings ToAudioProcessingSettings(
      bool multi_channel_capture_processing) const;

  // On Cobalt, audio processing (AEC, NS, AGC, etc.) is disabled by default to
  // avoid CPU-intensive tasks on embedded devices.
#if BUILDFLAG(IS_COBALT)
  EchoCancellationType echo_cancellation_type =
      EchoCancellationType::kEchoCancellationDisabled;
#else
  EchoCancellationType echo_cancellation_type =
      EchoCancellationType::kEchoCancellationAec3;
#endif
  // Indicates whether system-level gain control and noise suppression
  // functionalities are active that fill a role comparable to the browser
  // counterparts.
  bool system_gain_control_activated = false;
  bool system_noise_suppression_activated = false;

  // Used for an experiment for forcing certain system-level
  // noise suppression functionalities to be off. In contrast to
  // `system_noise_suppression_activated` the system-level noise suppression
  // referred to does not correspond to something that can replace the browser
  // counterpart. I.e., the browser counterpart should be on, even if
  // `disable_hw_noise_suppression` is false.
  bool disable_hw_noise_suppression = false;

  bool goog_audio_mirroring = false;
#if BUILDFLAG(IS_COBALT)
  bool goog_auto_gain_control = false;
#else
  bool goog_auto_gain_control = true;
#endif
  // TODO(https://crbug.com/1269723): Deprecate this constraint. The flag no
  // longer toggles meaningful processing effects, but it still forces the audio
  // processing module to be created and used.
  bool goog_experimental_echo_cancellation =
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_COBALT)
      false;
#else
      true;
#endif
#if BUILDFLAG(IS_COBALT)
  bool goog_noise_suppression = false;
  // Experimental noise suppression maps to transient suppression (keytap
  // removal).
  bool goog_experimental_noise_suppression = false;
  bool goog_highpass_filter = false;
#else
  bool goog_noise_suppression = true;
  // Experimental noise suppression maps to transient suppression (keytap
  // removal).
  bool goog_experimental_noise_suppression = true;
  bool goog_highpass_filter = true;
#endif
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_
