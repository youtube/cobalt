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

#ifndef COBALT_AUDIO_AUDIO_DEVICE_H_
#define COBALT_AUDIO_AUDIO_DEVICE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/media/base/shell_audio_bus.h"
#else  // defined(COBALT_MEDIA_SOURCE_2016)
#include "media/base/shell_audio_bus.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

namespace cobalt {
namespace audio {

class AudioDevice {
 public:
  class RenderCallback {
   public:
#if defined(COBALT_MEDIA_SOURCE_2016)
    typedef media::ShellAudioBus ShellAudioBus;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
    typedef ::media::ShellAudioBus ShellAudioBus;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

    // |silence| will be set to true before calling if |audio_buffer| contains
    // only silence samples, it will be set to |false| otherwise.  On return
    // FillAudioBus() will set |silence| to |false| if it has modified
    // |audio_buffer|.
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
