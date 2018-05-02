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

#ifndef STARBOARD_CONTRIB_TIZEN_SHARED_WAYLAND_APPLICATION_TIZEN_H_
#define STARBOARD_CONTRIB_TIZEN_SHARED_WAYLAND_APPLICATION_TIZEN_H_

#include <tizen-extension-client-protocol.h>

#include "starboard/shared/wayland/application_wayland.h"

namespace starboard {
namespace shared {
namespace wayland {

class ApplicationWaylandTizen : public ApplicationWayland {
 public:
  ApplicationWaylandTizen() {}
  ~ApplicationWaylandTizen() {}

  // wl registry add listener
  bool OnGlobalObjectAvailable(struct wl_registry* registry,
                               uint32_t name,
                               const char* interface,
                               uint32_t version);

  SbWindow CreateWindow(const SbWindowOptions* options);

 private:
  tizen_policy* tz_policy_;
};

}  // wayland
}  // shared
}  // starboard

#endif  // STARBOARD_CONTRIB_TIZEN_SHARED_WAYLAND_APPLICATION_TIZEN_H_
