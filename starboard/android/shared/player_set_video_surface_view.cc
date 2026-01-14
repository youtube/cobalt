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

#include "starboard/android/shared/player_set_video_surface_view.h"

#include "starboard/android/shared/video_surface_view.h"
#include "starboard/extension/player_set_video_surface_view.h"

namespace starboard::android::shared {

namespace {

const StarboardExtensionPlayerSetVideoSurfaceViewApi
    kPlayerSetVideoSurfaceView = {
        kStarboardExtensionPlayerSetVideoSurfaceViewName, 1,
        &SetVideoSurfaceViewForCurrentThread};

}  // namespace

const void* GetPlayerSetVideoSurfaceViewApi() {
  return &kPlayerSetVideoSurfaceView;
}

}  // namespace starboard::android::shared
