// Copyright 2016 Samsung Electronics. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WAYLAND_WINDOW_INTERNAL_H_
#define STARBOARD_SHARED_WAYLAND_WINDOW_INTERNAL_H_

#include <Elementary.h>
#include <string.h>
#include <tizen-extension-client-protocol.h>
#include <wayland-client.h>
#include <wayland-egl.h>

#include "starboard/window.h"

struct SbWindowPrivate {
  explicit SbWindowPrivate(const SbWindowOptions* options);
  ~SbWindowPrivate() {}

  struct wl_surface* surface;
  struct wl_shell_surface* shell_surface;
  struct wl_egl_window* egl_window;
  struct tizen_visibility* tz_visibility;

#if SB_CAN(USE_WAYLAND_VIDEO_WINDOW)
  wl_display* video_window;
#else
  Evas_Object* video_window;
#endif

  // The width, height of this window.
  int width;
  int height;
};

#endif  // STARBOARD_SHARED_WAYLAND_WINDOW_INTERNAL_H_
