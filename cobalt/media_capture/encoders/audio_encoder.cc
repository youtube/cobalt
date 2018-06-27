// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "cobalt/media_capture/encoders/audio_encoder.h"

#include <algorithm>

namespace cobalt {
namespace media_capture {
namespace encoders {

void AudioEncoder::AddListener(AudioEncoder::Listener* listener) {
  starboard::ScopedLock lock(listeners_mutex_);
  if (std::find(listeners_.begin(), listeners_.end(), listener) !=
      listeners_.end()) {
    return;
  }
  listeners_.push_back(listener);
}

void AudioEncoder::RemoveListener(AudioEncoder::Listener* listener) {
  starboard::ScopedLock lock(listeners_mutex_);
  listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), listener),
                   listeners_.end());
}

void AudioEncoder::PushDataToAllListeners(const uint8* data, size_t data_size,
                                          base::TimeTicks timecode) {
  starboard::ScopedLock lock(listeners_mutex_);
  for (AudioEncoder::Listener* listener : listeners_) {
    listener->OnEncodedDataAvailable(data, data_size, timecode);
  }
}

}  // namespace encoders
}  // namespace media_capture
}  // namespace cobalt
