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

#ifndef STARBOARD_TIZEN_SHARED_WINDOW_INTERNAL_H_
#define STARBOARD_TIZEN_SHARED_WINDOW_INTERNAL_H_

#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <string.h>

#include "starboard/window.h"

struct SbWindowPrivate {
  SbWindowPrivate(const SbWindowOptions* options);
  ~SbWindowPrivate() {};

  struct wl_surface *surface;
  struct wl_shell_surface *shell_surface;
  struct wl_egl_window *egl_window;

  // The width, height of this window.
  int width;
  int height;
};

#endif  // STARBOARD_TIZEN_SHARED_WINDOW_INTERNAL_H_
