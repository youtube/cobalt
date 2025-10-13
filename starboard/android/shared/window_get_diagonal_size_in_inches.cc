// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/display_util.h"
#include "starboard/android/shared/window_internal.h"
#include "starboard/common/log.h"

using starboard::DisplayUtil;

float SbWindowGetDiagonalSizeInInches(SbWindow window) {
  if (!SbWindowIsValid(window)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid window.";
    return 0.0f;
  }

  int32_t width_pixels = ANativeWindow_getWidth(window->native_window);
  int32_t height_pixels = ANativeWindow_getHeight(window->native_window);

  DisplayUtil::Dpi display_dpi = DisplayUtil::GetDisplayDpi();

  float xdpi = display_dpi.width;
  float ydpi = display_dpi.height;

  if (xdpi < 0.1f || ydpi < 0.1f) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid display values.";
    return 0.0f;
  }

  float width_inches = width_pixels / xdpi;
  float height_inches = height_pixels / ydpi;
  float diagonal_inches = sqrt(pow(width_inches, 2) + pow(height_inches, 2));
  return diagonal_inches;
}
