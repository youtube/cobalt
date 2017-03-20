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

#include "starboard/window.h"

#include "starboard/player.h"
#include "starboard/shared/x11/window_internal.h"

void* SbWindowGetPlatformHandle(SbWindow window) {
  if (!SbWindowIsValid(window)) {
    return NULL;
  }

  Window handle = None;
#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION || \
    SB_IS(PLAYER_PUNCHED_OUT)
  handle = window->gl_window;
#else
  handle = window->window;
#endif
  return reinterpret_cast<void*>(handle);
}
