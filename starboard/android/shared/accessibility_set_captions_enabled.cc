// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#if SB_API_VERSION < 16
#include "starboard/accessibility.h"
#else  // SB_API_VERSION < 16
#include "starboard/android/shared/accessibility_extension.h"
#endif  // SB_API_VERSION < 16
#include "starboard/configuration.h"

namespace starboard {
namespace android {
namespace shared {
namespace accessibility {

bool SetCaptionsEnabled(bool enabled) {
  return false;
}

}  // namespace accessibility
}  // namespace shared
}  // namespace android
}  // namespace starboard

#if SB_API_VERSION < 16
bool SbAccessibilitySetCaptionsEnabled(bool enabled) {
  return starboard::android::shared::accessibility::SetCaptionsEnabled(enabled);
}
#endif  // SB_API_VERSION < 16
