// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_AUDIO_AUDIO_DEVICE_H_
#define COBALT_AUDIO_AUDIO_DEVICE_H_

#include <memory>
#include <vector>

#include "base/basictypes.h"
#include "cobalt/media/base/audio_bus.h"

namespace cobalt {
namespace audio {

class AudioDevice {
 public:
  class RenderCallback {
   public:
    typedef media::AudioBus AudioBus;

    // |all_consumed| will be set to true if all audio frames has been consumed.
    // This gives the AudioDestinationNode a chance to decide if the AudioDevice
    // should be killed.
    // |audio_buffer| contains the audio frames to be mixed with input audio if
    // there is any.
    // |silence| will be set to true before calling if |audio_buffer| contains
    // only silence samples, it will be set to |false| otherwise.  It will be
    // set to false on return if |audio_buffer| has been modified.
    virtual void FillAudioBus(bool all_consumed, AudioBus* audio_buffer,
                              bool* silence) = 0;

   protected:
    ~RenderCallback() {}
  };

  AudioDevice(int32 number_of_channels, RenderCallback* callback);
  ~AudioDevice();

 private:
  class Impl;
  const std::unique_ptr<Impl> impl_;

  DISALLOW_COPY_AND_ASSIGN(AudioDevice);
};

}  // namespace audio
}  // namespace cobalt

#endif  // COBALT_AUDIO_AUDIO_DEVICE_H_
