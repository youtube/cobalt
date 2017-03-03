// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/h5vcc/h5vcc_accessibility.h"

#include "starboard/accessibility.h"
#include "starboard/memory.h"

namespace cobalt {
namespace h5vcc {

H5vccAccessibility::H5vccAccessibility() : high_contrast_text_(false) {
#if SB_API_VERSION >= SB_EXPERIMENTAL_API_VERSION
  SbAccessibilityDisplaySettings settings;
  SbMemorySet(&settings, 0, sizeof(settings));

  if (!SbAccessibilityGetDisplaySettings(&settings)) {
    return;
  }

  high_contrast_text_ = settings.is_high_contrast_text_enabled;
#else  // SB_API_VERSION >= SB_EXPERIMENTAL_API_VERSION
  high_contrast_text_ = false;
#endif  // SB_API_VERSION >= SB_EXPERIMENTAL_API_VERSION
}

bool H5vccAccessibility::high_contrast_text() const {
  return high_contrast_text_;
}

}  // namespace h5vcc
}  // namespace cobalt
