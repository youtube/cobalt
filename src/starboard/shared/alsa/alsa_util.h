// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_ALSA_ALSA_UTIL_H_
#define STARBOARD_SHARED_ALSA_ALSA_UTIL_H_

#include <alsa/asoundlib.h>

#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace alsa {

// The lengthy ALSA device opening function.
// TODO: Use native alsa period as |frames_per_request|.
void* AlsaOpenPlaybackDevice(int channel,
                             int sample_rate,
                             int frames_per_request,
                             int buffer_size_in_frames,
                             snd_pcm_format_t sample_type);
bool AlsaStartPlay(void* playback_handle);
int AlsaWriteFrames(void* playback_handle,
                    const void* buffer,
                    int frames_to_write);
int AlsaGetBufferedFrames(void* playback_handle);
void AlsaCloseDevice(void* playback_handle);
bool AlsaDrain(void* playback_handle);

}  // namespace alsa
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_ALSA_ALSA_UTIL_H_
