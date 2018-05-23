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

#include "starboard/contrib/tizen/shared/wayland/window_internal_tizen.h"

#include "starboard/shared/wayland/application_wayland.h"

static void WindowCbVisibilityChange(void* data,
                                     struct tizen_visibility* tizen_visibility
                                         EINA_UNUSED,
                                     uint32_t visibility) {
#if SB_HAS(LAZY_SUSPEND)
  if (visibility == TIZEN_VISIBILITY_VISIBILITY_FULLY_OBSCURED)
    ApplicationWayland::Get()->Pause(NULL, NULL);
  else
    ApplicationWayland::Get()->Unpause(NULL, NULL);
#else
  if (visibility == TIZEN_VISIBILITY_VISIBILITY_FULLY_OBSCURED)
    starboard::shared::starboard::Application::Get()->Suspend(NULL, NULL);
  else
    starboard::shared::starboard::Application::Get()->Unpause(NULL, NULL);
#endif
}

static const struct tizen_visibility_listener tizen_visibility_listener = {
    WindowCbVisibilityChange
};

SbWindowPrivateTizen::SbWindowPrivateTizen(wl_display* display,
                                           tizen_policy* policy,
                                           wl_compositor* compositor,
                                           wl_shell* shell,
                                           const SbWindowOptions* options,
                                           float pixel_ratio)
    : SbWindowPrivate(compositor, shell, options, pixel_ratio) {
#if SB_CAN(USE_WAYLAND_VIDEO_WINDOW)
  video_window = display;
#else
  video_window = elm_win_add(NULL, "Cobalt_Video", ELM_WIN_BASIC);
  elm_win_title_set(video_window, "Cobalt_Video");
  elm_win_autodel_set(video_window, EINA_TRUE);
  evas_object_resize(video_window, width, height);
  evas_object_hide(video_window);
#endif
  tz_policy = policy;
  tz_visibility = tizen_policy_get_visibility(tz_policy, surface);
  tizen_visibility_add_listener(tz_visibility, &tizen_visibility_listener,
                                this);
  tizen_policy_activate(tz_policy, surface);
}

SbWindowPrivateTizen::~SbWindowPrivateTizen() {
#if !SB_CAN(USE_WAYLAND_VIDEO_WINDOW)
  evas_object_hide(video_window);
#endif
  video_window = NULL;
  tizen_visibility_destroy(tz_visibility);
}

void SbWindowPrivateTizen::WindowRaise() {
  SbWindowPrivate::WindowRaise();
  if (tz_policy)
    tizen_policy_raise(tz_policy, surface);
}
