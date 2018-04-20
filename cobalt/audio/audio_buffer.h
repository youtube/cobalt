// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_AUDIO_AUDIO_BUFFER_H_
#define COBALT_AUDIO_AUDIO_BUFFER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"  // For scoped_array
#include "cobalt/audio/audio_helpers.h"
#include "cobalt/dom/float32_array.h"
#include "cobalt/dom/int16_array.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace audio {

// This represents a memory-resident audio asset (for one-shot sounds
// and other short audio clips). Its format is non-interleaved IEEE 32-bit
// linear PCM with a nominal range of -1 -> +1. It can contain one or more
// channels. Typically, it would be expected that the length of the PCM data
// would be fairly short (usually somewhat less than a minute). For longer
// sounds, such as music soundtracks, streaming should be used with the audio
// element and MediaElementAudioSourceNode.
// An AudioBuffer may be used by one or more AudioContexts.
//   https://www.w3.org/TR/webaudio/#AudioBuffer
class AudioBuffer : public script::Wrappable {
 public:
  AudioBuffer(float sample_rate, scoped_ptr<ShellAudioBus> audio_bus);

  // Web API: AudioBuffer
  //
  // The sample-rate for the PCM audio data in samples per second.
  float sample_rate() const { return sample_rate_; }
  // Length of the PCM audio data in sample-frames.
  int32 length() const { return static_cast<int32>(audio_bus_->frames()); }
  // Duration of the PCM audio data in seconds.
  double duration() const { return length() / sample_rate(); }
  // The number of discrete audio channels.
  int32 number_of_channels() const {
    return static_cast<int32>(audio_bus_->channels());
  }

  // Custom, not in any spec
  //
  ShellAudioBus* audio_bus() { return audio_bus_.get(); }

  DEFINE_WRAPPABLE_TYPE(AudioBuffer);

 private:
  const float sample_rate_;

  scoped_ptr<ShellAudioBus> audio_bus_;

  DISALLOW_COPY_AND_ASSIGN(AudioBuffer);
};

}  // namespace audio
}  // namespace cobalt

#endif  // COBALT_AUDIO_AUDIO_BUFFER_H_
