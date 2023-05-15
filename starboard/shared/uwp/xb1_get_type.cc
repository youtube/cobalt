// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
#include "starboard/shared/uwp/xb1_get_type.h"

#include <string>

#include "starboard/system.h"

namespace starboard {
namespace shared {
namespace uwp {
XboxType GetXboxType() {
  // The value kXboxUndefined means that the model name needs to be clarified at
  // the first time and cached for further calls.
  static XboxType xbox_type = kXboxUndefined;
  constexpr size_t kNameLength = 1024;
  char name[kNameLength] = {};

  if (xbox_type == kXboxUndefined) {
    // Provide a base functionality even for unknown models.
    xbox_type = kXboxOneBase;

    // Detect from system properties which model runs the application.
    if (SbSystemGetProperty(kSbSystemPropertyModelName, name, kNameLength)) {
      const std::string friendly_name(name);
      if (friendly_name == "XboxOne S") {
        xbox_type = kXboxOneS;
      } else if (friendly_name == "XboxOne X") {
        xbox_type = kXboxOneX;
      } else if (friendly_name == "XboxScarlett Series S") {
        xbox_type = kXboxSeriesS;
      } else if (friendly_name == "XboxScarlett Series X") {
        xbox_type = kXboxSeriesX;
      }
    }
  }
  return xbox_type;
}
}  // namespace uwp
}  // namespace shared
}  // namespace starboard
