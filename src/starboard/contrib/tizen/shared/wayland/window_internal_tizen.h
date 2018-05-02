// Copyright 2018 Samsung Electronics. All Rights Reserved.
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

#ifndef STARBOARD_CONTRIB_TIZEN_SHAREDWAYLAND_WINDOW_INTERNAL_TIZEN_H_
#define STARBOARD_CONTRIB_TIZEN_SHAREDWAYLAND_WINDOW_INTERNAL_TIZEN_H_

#include <Elementary.h>
#include <tizen-extension-client-protocol.h>

#include "starboard/shared/wayland/window_internal.h"

struct SbWindowPrivateTizen : SbWindowPrivate {
  explicit SbWindowPrivateTizen(wl_display* display,
                                tizen_policy* policy,
                                wl_compositor* compositor,
                                wl_shell* shell,
                                const SbWindowOptions* options,
                                float pixel_ratio = 1.0);

  ~SbWindowPrivateTizen();

  void WindowRaise() override;

  tizen_policy* tz_policy;
  tizen_visibility* tz_visibility;
#if SB_CAN(USE_WAYLAND_VIDEO_WINDOW)
  wl_display* video_window;
#else
  Evas_Object* video_window;
#endif
};

#endif  // STARBOARD_CONTRIB_TIZEN_SHAREDWAYLAND_WINDOW_INTERNAL_TIZEN_H_
