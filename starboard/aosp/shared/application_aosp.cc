// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/aosp/shared/application_aosp.h"

#include <android/native_window.h>

#include "starboard/aosp/shared/window_internal.h"
#include "starboard/aosp/shared/window_surface.h"
#include "starboard/common/log.h"
#include "starboard/window.h"

namespace starboard {

SbWindow ApplicationAOSP::CreateWindow(const SbWindowOptions* /*options*/) {
  ANativeWindow* native_window = android::shared::AcquireWindowSurface();
  if (native_window == nullptr) {
    SB_LOG(ERROR) << "SbWindowCreate: no Android surface available.";
    return kSbWindowInvalid;
  }
  SbWindow window = new SbWindowPrivate();
  window->native_window = native_window;
  return window;
}

bool ApplicationAOSP::DestroyWindow(SbWindow window) {
  if (!SbWindowIsValid(window)) {
    return false;
  }
  ANativeWindow_release(window->native_window);
  delete window;
  return true;
}

}  // namespace starboard
