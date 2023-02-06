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

#include "starboard/shared/win32/graphics.h"

#include "starboard/extension/graphics.h"

namespace starboard {
namespace shared {
namespace win32 {

namespace {

float GetMaximumFrameIntervalInMilliseconds() {
  // Allow the rasterizer to delay rendering indefinitely if nothing has
  // changed.
  return -1.0f;
}

float GetMinimumFrameIntervalInMilliseconds() {
  // Frame presentation is blocked on vsync, so the render thread will also
  // block on vsync. However, use a non-zero minimum frame time to avoid
  // possible busy-loops on unrendered submissions.
  return 1.0f;
}

const CobaltExtensionGraphicsApi kGraphicsApi = {
    kCobaltExtensionGraphicsName,
    2,
    &GetMaximumFrameIntervalInMilliseconds,
    &GetMinimumFrameIntervalInMilliseconds,
};

}  // namespace

const void* GetGraphicsApi() {
  return &kGraphicsApi;
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
