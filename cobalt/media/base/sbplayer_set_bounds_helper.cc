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

#include "cobalt/media/base/sbplayer_set_bounds_helper.h"

#include "base/atomic_sequence_num.h"
#include "cobalt/media/base/starboard_player.h"

namespace cobalt {
namespace media {

namespace {
// StaticAtomicSequenceNumber is safe to be initialized statically.
base::StaticAtomicSequenceNumber s_z_index;
}  // namespace

void SbPlayerSetBoundsHelper::SetPlayer(StarboardPlayer* player) {
  base::AutoLock auto_lock(lock_);
  player_ = player;
}

bool SbPlayerSetBoundsHelper::SetBounds(const gfx::Rect& rect) {
  base::AutoLock auto_lock(lock_);
  if (!player_) {
    return false;
  }
  player_->SetBounds(s_z_index.GetNext(), rect);
  return true;
}

}  // namespace media
}  // namespace cobalt
