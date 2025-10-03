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

#ifndef MEDIA_STARBOARD_SBPLAYER_SET_BOUNDS_HELPER_H_
#define MEDIA_STARBOARD_SBPLAYER_SET_BOUNDS_HELPER_H_

#include <optional>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "ui/gfx/geometry/rect.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {

class SbPlayerBridge;

// TODO: b/448196546 - Fold this class into SbPlayerBridge.
class SbPlayerSetBoundsHelper {
 public:
  explicit SbPlayerSetBoundsHelper(base::SequencedTaskRunner* task_runner);

  void SetPlayerBridge(SbPlayerBridge* player_bridge);
  bool SetBounds(const gfx::Rect& rect);

  base::WeakPtr<SbPlayerSetBoundsHelper> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SbPlayerBridge* player_bridge_ = nullptr;
  std::optional<gfx::Rect> rect_;

  SbPlayerSetBoundsHelper(const SbPlayerSetBoundsHelper&) = delete;
  void operator=(const SbPlayerSetBoundsHelper&) = delete;

  base::WeakPtrFactory<SbPlayerSetBoundsHelper> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_STARBOARD_SBPLAYER_SET_BOUNDS_HELPER_H_
