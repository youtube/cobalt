// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/shared/starboard/player/decoded_audio_internal.h"

#include "starboard/log.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

DecodedAudio::DecodedAudio() : pts_(0), size_(0) {}

DecodedAudio::DecodedAudio(SbMediaTime pts, size_t size)
    : pts_(pts), buffer_(new uint8_t[size]), size_(size) {
  SB_DCHECK(size > 0) << size;
}

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
