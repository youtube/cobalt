// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <utility>

#include "base/atomic_sequence_num.h"
#include "cobalt/media/base/sbplayer_bridge.h"

namespace cobalt {
namespace media {

namespace {
// StaticAtomicSequenceNumber is safe to be initialized statically.
//
// Cobalt renderer renders from back to front, using a monotonically increasing
// sequence guarantees that all video layers are correctly ordered on z axis.
base::AtomicSequenceNumber s_z_index;
}  // namespace

void SbPlayerSetBoundsHelper::SetCallback(SetBoundsCB set_bounds_cb) {
  base::AutoLock auto_lock(lock_);
  set_bounds_cb_ = std::move(set_bounds_cb);
  if (!set_bounds_cb_.is_null() && rect_.has_value()) {
    set_bounds_cb_.Run(s_z_index.GetNext(), rect_.value());
  }
}

void SbPlayerSetBoundsHelper::ResetCallback() {
  base::AutoLock auto_lock(lock_);
  set_bounds_cb_ = SetBoundsCB();
}

bool SbPlayerSetBoundsHelper::SetBounds(int x, int y, int width, int height) {
  base::AutoLock auto_lock(lock_);
  rect_ = gfx::Rect(x, y, width, height);
  if (!set_bounds_cb_.is_null()) {
    set_bounds_cb_.Run(s_z_index.GetNext(), rect_.value());
  }

  return !set_bounds_cb_.is_null();
}

}  // namespace media
}  // namespace cobalt
