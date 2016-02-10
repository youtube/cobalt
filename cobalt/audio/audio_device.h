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

#ifndef COBALT_AUDIO_AUDIO_DEVICE_H_
#define COBALT_AUDIO_AUDIO_DEVICE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/shell_audio_bus.h"

namespace cobalt {
namespace audio {

class AudioDevice {
 public:
  class RenderCallback {
   public:
    typedef ::media::ShellAudioBus ShellAudioBus;
    virtual void FillAudioBus(ShellAudioBus* audio_buffer, bool* silence) = 0;

   protected:
    ~RenderCallback() {}
  };

  AudioDevice(int32 number_of_channels, RenderCallback* callback);
  ~AudioDevice();

 private:
  class Impl;
  const scoped_ptr<Impl> impl_;

  DISALLOW_COPY_AND_ASSIGN(AudioDevice);
};

}  // namespace audio
}  // namespace cobalt

#endif  // COBALT_AUDIO_AUDIO_DEVICE_H_
