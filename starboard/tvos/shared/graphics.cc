// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/tvos/shared/graphics.h"

#include "starboard/extension/graphics.h"

namespace starboard {
namespace shared {
namespace uikit {

namespace {

float GetMaximumFrameIntervalInMilliseconds() {
  // Allow the rasterizer to delay rendering indefinitely if nothing has
  // changed.
  return -1.0f;
}

float GetMinimumFrameIntervalInMilliseconds() {
  // Let vsync throttle presentation of new frames. Greater values risk
  // dropping frames when watching videos with decode-to-target. Lower values
  // will increase CPU usage if new frames are not submitted each iteration.
  return 8.0f;
}

bool IsMapToMeshEnabled() {
  // Disable map to mesh, as we don't want to support 360 degree video on tvos.
  return false;
}

bool ShouldClearFrameOnShutdown(float* clear_color_red,
                                float* clear_color_green,
                                float* clear_color_blue,
                                float* clear_color_alpha) {
  *clear_color_red = 0.0f;
  *clear_color_green = 0.0f;
  *clear_color_blue = 0.0f;
  *clear_color_alpha = 1.0f;
  return true;
}

const CobaltExtensionGraphicsApi kGraphicsApi = {
    kCobaltExtensionGraphicsName,
    4,
    &GetMaximumFrameIntervalInMilliseconds,
    &GetMinimumFrameIntervalInMilliseconds,
    &IsMapToMeshEnabled,
    &ShouldClearFrameOnShutdown,
};

}  // namespace

const void* GetGraphicsApi() {
  return &kGraphicsApi;
}

}  // namespace uikit
}  // namespace shared
}  // namespace starboard
