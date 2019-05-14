// Copyright 2018 Samsung Inc. All Rights Reserved.
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

#include "starboard/shared/wayland/native_display_type.h"

#include "starboard/common/log.h"
#include "starboard/shared/wayland/application_wayland.h"

NativeDisplayType WaylandNativeDisplayType() {
  wl_display* display =
      starboard::shared::wayland::ApplicationWayland::Get()->GetWLDisplay();
  SB_LOG(INFO) << " SbNativeDisplayType() " << display;
  return (NativeDisplayType)display;
}
