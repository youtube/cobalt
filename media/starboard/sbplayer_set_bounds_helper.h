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

#ifndef COBALT_MEDIA_BASE_SBPLAYER_SET_BOUNDS_HELPER_H_
#define COBALT_MEDIA_BASE_SBPLAYER_SET_BOUNDS_HELPER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "ui/gfx/geometry/rect.h"

namespace cobalt {
namespace media {

class SbPlayerBridge;

class SbPlayerSetBoundsHelper
    : public base::RefCountedThreadSafe<SbPlayerSetBoundsHelper> {
 public:
  SbPlayerSetBoundsHelper() {}

  void SetPlayerBridge(SbPlayerBridge* player_bridge);
  bool SetBounds(int x, int y, int width, int height);

 private:
  base::Lock lock_;
  SbPlayerBridge* player_bridge_ = nullptr;
  base::Optional<gfx::Rect> rect_;

  DISALLOW_COPY_AND_ASSIGN(SbPlayerSetBoundsHelper);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_SBPLAYER_SET_BOUNDS_HELPER_H_
