// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_AUDIO_SINK_ANDROID_H_
#define STARBOARD_ANDROID_SHARED_AUDIO_SINK_ANDROID_H_

#include <cstdint>

#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

namespace starboard {

// AudioSinkAndroid is an abstract interface for Android-specific audio sinks.
// It extends SbAudioSinkImpl to provide additional control methods required
// by AudioRendererSinkAndroid, such as flushing and setting start time.
//
// Data Flow Architecture:
// This interface does not provide explicit Write() or Append() methods.
// Instead, audio data exchange is continuous and callback-driven, established
// at creation time via SbAudioSinkPrivate::Type::Create():
//   1. The caller provides a shared circular buffer (SbAudioSinkFrameBuffers).
//   2. The sink queries available frames and playback state via
//      SbAudioSinkUpdateSourceStatusFunc.
//   3. The sink consumes audio directly from the shared buffer (pushing to
//      AudioTrack, or pulled by an AAudio data callback) and reports
//      consumption via ConsumeFramesFunc.
//
// Lifetime and Ownership:
// It is created by the preferred SbAudioSinkPrivate::Type and owned by
// AudioRendererSinkAndroid.
//
// Threading Model:
// Implementations must be thread-safe or coordinate access between the player
// thread and the audio output thread.
class AudioSinkAndroid : public SbAudioSinkImpl {
 public:
  ~AudioSinkAndroid() override = default;

  virtual void SetStartTime(int64_t start_time_us) = 0;
  virtual bool Flush() = 0;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_AUDIO_SINK_ANDROID_H_
