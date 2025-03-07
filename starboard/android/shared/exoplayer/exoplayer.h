// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_H_
#define STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_H_

#include <memory>

#include "starboard/android/shared/exoplayer/exoplayer_bridge.h"
#include "starboard/player.h"

class ExoPlayer final {
 public:
  static SbPlayerPrivate* CreateInstance();
  static ExoPlayer* GetExoPlayerForSbPlayer(SbPlayer player);
  ~ExoPlayer();

 private:
  ExoPlayer();

  std::unique_ptr<ExoPlayerBridge> bridge_;
};

#endif  // STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_H_
