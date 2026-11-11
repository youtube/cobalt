// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

// clang-format off
#include "starboard/player.h"
// clang-format on

#include <jni.h>

#include <mutex>
#include <optional>

#include "starboard/android/shared/protected_video_plane.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/log.h"
#include "starboard/common/rect.h"
#include "starboard/shared/starboard/player/player_internal.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {
namespace {

std::mutex& BoundsMutex() {
  static std::mutex mutex;
  return mutex;
}
std::optional<Rect>& LastBounds() {
  static std::optional<Rect> bounds;
  return bounds;
}

}  // namespace

void SetVideoSurfaceFullscreen() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  auto* bridge = StarboardBridge::GetInstance();
  const Size size = bridge->GetDeviceResolution(env);
  if (size.width > 0 && size.height > 0) {
    bridge->SetVideoSurfaceBounds(env, 0, 0, size.width, size.height);
  }
}

void ReapplyVideoSurfaceBounds() {
  std::optional<Rect> bounds;
  {
    std::lock_guard lock(BoundsMutex());
    bounds = LastBounds();
  }
  if (!bounds) {
    return;
  }
  JNIEnv* env = jni_zero::AttachCurrentThread();
  StarboardBridge::GetInstance()->SetVideoSurfaceBounds(
      env, bounds->x, bounds->y, bounds->size.width, bounds->size.height);
}

}  // namespace starboard

void SbPlayerSetBounds(SbPlayer player,
                       int z_index,
                       int x,
                       int y,
                       int width,
                       int height) {
  if (!SbPlayerIsValid(player)) {
    SB_DLOG(WARNING) << "player is invalid.";
    return;
  }

  {
    std::lock_guard lock(starboard::BoundsMutex());
    starboard::LastBounds() = starboard::Rect(x, y, width, height);
  }
  // A protected presenter draws the hole itself, into a fullscreen surface.
  // Resizing that surface blanks the plane, which turns squeezeback black.
  if (starboard::IsVideoWindowHeldByPresenter()) {
    starboard::SetVideoSurfaceFullscreen();
  } else {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    starboard::StarboardBridge::GetInstance()->SetVideoSurfaceBounds(
        env, x, y, width, height);
  }
  player->SetBounds(z_index, starboard::Rect(x, y, width, height));
}
