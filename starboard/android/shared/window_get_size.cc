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

#include <android/native_window.h>

#include "starboard/android/shared/window_internal.h"
#include "starboard/log.h"
#include "starboard/window.h"

bool SbWindowGetSize(SbWindow window, SbWindowSize* size) {
  if (!SbWindowIsValid(window)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid window.";
    return false;
  }

  size->width = ANativeWindow_getWidth(window->native_window);
  size->height = ANativeWindow_getHeight(window->native_window);

  // TODO: Read Android property sys.display-size and compare to window size
  size->video_pixel_ratio = 1.0f;

  return true;
}
