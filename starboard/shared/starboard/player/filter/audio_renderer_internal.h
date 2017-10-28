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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RENDERER_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RENDERER_INTERNAL_H_

#include "starboard/common/ref_counted.h"
#include "starboard/configuration.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

class AudioRenderer {
 public:
  virtual ~AudioRenderer() {}

  virtual void Initialize(const Closure& error_cb) {
    SB_UNREFERENCED_PARAMETER(error_cb);
  }
  virtual void WriteSample(const scoped_refptr<InputBuffer>& input_buffer) = 0;
  virtual void WriteEndOfStream() = 0;
  virtual void Play() = 0;
  virtual void Pause() = 0;
  virtual void SetPlaybackRate(double playback_rate) = 0;
  virtual void SetVolume(double volume) = 0;
  virtual void Seek(SbMediaTime seek_to_pts) = 0;
  virtual bool IsEndOfStreamWritten() const = 0;
  virtual bool IsEndOfStreamPlayed() const = 0;
  // TODO: Replace polling with callbacks.
  virtual bool CanAcceptMoreData() const = 0;
  virtual bool IsSeekingInProgress() const = 0;
  virtual SbMediaTime GetCurrentTime() = 0;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RENDERER_INTERNAL_H_
