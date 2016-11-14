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

#ifndef MEDIA_BASE_SBPLAYER_SET_BOUNDS_HELPER_H_
#define MEDIA_BASE_SBPLAYER_SET_BOUNDS_HELPER_H_

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "ui/gfx/rect.h"

namespace media {

class StarboardPlayer;

class SbPlayerSetBoundsHelper
    : public base::RefCountedThreadSafe<SbPlayerSetBoundsHelper> {
 public:
  SbPlayerSetBoundsHelper() : player_(NULL) {}

  void SetPlayer(StarboardPlayer* player);
  bool SetBounds(const gfx::Rect& rect);

 private:
  base::Lock lock_;
  StarboardPlayer* player_;

  DISALLOW_COPY_AND_ASSIGN(SbPlayerSetBoundsHelper);
};

}  // namespace media

#endif  // MEDIA_BASE_SBPLAYER_SET_BOUNDS_HELPER_H_
