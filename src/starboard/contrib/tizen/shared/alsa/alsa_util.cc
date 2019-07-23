// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/contrib/tizen/shared/alsa/alsa_util.h"

#include <pulse/error.h>
#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace alsa {

void* AlsaOpenPlaybackDevice(int channel,
                             int sample_rate,
                             pa_sample_format sample_type) {
  const pa_sample_spec ss = {
      .format = sample_type, .rate = sample_rate, .channels = channel};
  int error;
  pa_simple* playback_handle =
      pa_simple_new(NULL, "cobalt", PA_STREAM_PLAYBACK, NULL, "playback", &ss,
                    NULL, NULL, &error);
  if (!playback_handle) {
    SB_LOG(ERROR) << " pa_simple_new failed " << pa_strerror(error);
  }

  return playback_handle;
}

bool AlsaWriteFrames(void* playback_handle, const void* buffer, size_t bytes) {
  if (bytes == 0) {
    return true;
  }

  int error;
  pa_simple* handle = reinterpret_cast<pa_simple*>(playback_handle);
  if (pa_simple_write(handle, buffer, (size_t)bytes, &error) < 0) {
    SB_LOG(ERROR) << " pa_simple_write failed " << pa_strerror(error);
    return false;
  }

  return true;
}

int AlsaGetBufferedFrames(void* playback_handle) {
  pa_usec_t latency;
  int error;
  latency = pa_simple_get_latency(reinterpret_cast<pa_simple*>(playback_handle),
                                  &error);
  if (latency != (pa_usec_t)-1)
    return (int)latency;
  SB_LOG(ERROR) << " pa_simple_get_latency failed " << pa_strerror(error);
  return 0;
}

void AlsaCloseDevice(void* playback_handle) {
  if (playback_handle) {
    int error;
    if (pa_simple_drain(reinterpret_cast<pa_simple*>(playback_handle), &error) <
        0) {
      SB_LOG(ERROR) << " pa_simple_drain failed " << pa_strerror(error);
    }
    pa_simple_free(reinterpret_cast<pa_simple*>(playback_handle));
  }
}

bool AlsaDrain(void* playback_handle) {
  if (playback_handle) {
    int error;
    if (pa_simple_drain(reinterpret_cast<pa_simple*>(playback_handle), &error) <
        0) {
      SB_LOG(ERROR) << " pa_simple_drain failed " << pa_strerror(error);
      return false;
    }
  }
  return true;
}

}  // namespace alsa
}  // namespace shared
}  // namespace starboard
