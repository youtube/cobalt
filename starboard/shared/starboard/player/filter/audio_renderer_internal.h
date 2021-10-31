// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/common.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

class AudioRenderer {
 public:
  typedef ::starboard::shared::starboard::player::filter::EndedCB EndedCB;
  typedef ::starboard::shared::starboard::player::filter::ErrorCB ErrorCB;
  typedef ::starboard::shared::starboard::player::filter::PrerolledCB
      PrerolledCB;
  typedef ::starboard::shared::starboard::player::InputBuffer InputBuffer;

  virtual ~AudioRenderer() {}

  virtual void Initialize(const ErrorCB& error_cb,
                          const PrerolledCB& prerolled_cb,
                          const EndedCB& ended_cb) = 0;
  virtual void WriteSample(const scoped_refptr<InputBuffer>& input_buffer) = 0;
  virtual void WriteEndOfStream() = 0;

  virtual void SetVolume(double volume) = 0;

  // TODO: Remove the eos state querying functions and their tests.
  virtual bool IsEndOfStreamWritten() const = 0;
  virtual bool IsEndOfStreamPlayed() const = 0;
  virtual bool CanAcceptMoreData() const = 0;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RENDERER_INTERNAL_H_
