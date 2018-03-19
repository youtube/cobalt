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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_MEDIA_TIME_PROVIDER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_MEDIA_TIME_PROVIDER_H_

#include "starboard/media.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

class MediaTimeProvider {
 public:
  virtual void Play() = 0;
  virtual void Pause() = 0;
  virtual void SetPlaybackRate(double playback_rate) = 0;
  virtual void Seek(SbTime seek_to_pts) = 0;
  // This function can be called from *any* thread.
  virtual SbTime GetCurrentMediaTime(bool* is_playing, bool* is_eos_played) = 0;

 protected:
  virtual ~MediaTimeProvider() {}
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_MEDIA_TIME_PROVIDER_H_
