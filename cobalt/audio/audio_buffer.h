/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AUDIO_AUDIO_BUFFER_H_
#define AUDIO_AUDIO_BUFFER_H_

#include "base/memory/ref_counted.h"
#include "cobalt/dom/float32_array.h"
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
//   http://www.w3.org/TR/webaudio/#AudioBuffer
class AudioBuffer : public script::Wrappable {
 public:
  AudioBuffer(float sample_rate, int32 number_of_frames,
              int32 number_of_channels);

  // Web API: AudioBuffer
  //
  // The sample-rate for the PCM audio data in samples per second.
  float sample_rate() const { return sample_rate_; }

  // Length of the PCM audio data in sample-frames.
  int32 length() const { return length_; }

  // Duration of the PCM audio data in seconds.
  double duration() const { return length() / sample_rate(); }

  // The number of discrete audio channels.
  int32 number_of_channels() const { return number_of_channels_; }

  // Represents the PCM audio data for the specific channel.
  dom::Float32Array* GetChannelData(uint32 channel_index,
                                    script::ExceptionState* exception_state);

  DEFINE_WRAPPABLE_TYPE(AudioBuffer);

 private:
  float sample_rate_;
  int32 length_;
  int32 number_of_channels_;

  DISALLOW_COPY_AND_ASSIGN(AudioBuffer);
};

}  // namespace audio
}  // namespace cobalt

#endif  // AUDIO_AUDIO_BUFFER_H_
