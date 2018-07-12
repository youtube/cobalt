// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/contrib/tizen/shared/wayland/application_tizen.h"

#include <string.h>

#include "starboard/contrib/tizen/shared/wayland/window_internal_tizen.h"

namespace starboard {
namespace shared {
namespace wayland {

void ApplicationWaylandTizen::Initialize() {
  elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
  ApplicationWayland::Initialize();
}

bool ApplicationWaylandTizen::OnGlobalObjectAvailable(
    struct wl_registry* registry,
    uint32_t name,
    const char* interface,
    uint32_t version) {
  if (!strcmp(interface, "tizen_policy")) {
    tz_policy_ = static_cast<tizen_policy*>(wl_registry_bind(
        registry, name, &tizen_policy_interface, SB_TIZEN_POLICY_VERSION));
    return true;
  }

  return ApplicationWayland::OnGlobalObjectAvailable(registry, name, interface,
                                                     version);
}

SbWindow ApplicationWaylandTizen::CreateWindow(const SbWindowOptions* options) {
  SbWindow window = new SbWindowPrivateTizen(
      display_, tz_policy_, compositor_, shell_, options, video_pixel_ratio_);
  dev_input_.SetSbWindow(window);
  return window;
}

}  // namespace wayland
}  // namespace shared
}  // namespace starboard
