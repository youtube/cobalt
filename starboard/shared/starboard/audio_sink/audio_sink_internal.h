// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_SHARED_STARBOARD_AUDIO_SINK_AUDIO_SINK_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_AUDIO_SINK_AUDIO_SINK_INTERNAL_H_

#include "starboard/audio_sink.h"
#include "starboard/configuration.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_type.h"

struct SbAudioSinkPrivate {
  typedef ::starboard::shared::starboard::audio_sink::AudioSinkType
      AudioSinkType;
  virtual ~SbAudioSinkPrivate() {}
  virtual void SetPlaybackRate(double playback_rate) = 0;

  virtual void SetVolume(double volume) = 0;

  virtual bool IsAudioSinkType(const AudioSinkType* type) const = 0;

  // The following two functions will be called during application startup and
  // termination.
  static void Initialize();
  static void TearDown();
  // This function has to be called inside PlatformInitialize() and
  // PlatformTearDown() to set or clear the primary audio sink type.
  static void SetPrimaryAudioSinkType(AudioSinkType* type);
  static AudioSinkType* GetPrimaryAudioSinkType();
  // This function can only be called inside PlatformInitialize().  Once it is
  // called, GetFallbackAudioSinkType() will return a valid AudioSinkType that
  // can be used to create audio sink stubs.
  static void EnableFallbackToStub();
  // Return a valid AudioSinkType to create a fallback audio sink if fallback to
  // stub is enabled.  Otherwise return NULL.
  static AudioSinkType* GetFallbackAudioSinkType();

  // Return a valid Type, choosing the Primary type if available, otherwise
  // Fallback. If Fallback is not enabled, then returns NULL.
  static AudioSinkType* GetPreferredAudioSinkType();

  // Individual implementation has to provide implementation of the following
  // functions, which will be called inside Initialize() and TearDown().
  static void PlatformInitialize();
  static void PlatformTearDown();
};

#endif  // STARBOARD_SHARED_STARBOARD_AUDIO_SINK_AUDIO_SINK_INTERNAL_H_
