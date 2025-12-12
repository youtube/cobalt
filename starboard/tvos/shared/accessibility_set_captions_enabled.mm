// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#import <MediaAccessibility/MediaAccessibility.h>

#include "starboard/tvos/shared/accessibility_extension.h"

namespace starboard {
namespace shared {
namespace uikit {

namespace accessibility {
namespace {

constexpr MACaptionAppearanceDomain kUserDomain =
    kMACaptionAppearanceDomainUser;

}  // namespace

bool SetCaptionsEnabled(bool enabled) {
  if (enabled) {
    MACaptionAppearanceSetDisplayType(kUserDomain,
                                      kMACaptionAppearanceDisplayTypeAlwaysOn);
  } else {
    MACaptionAppearanceSetDisplayType(
        kUserDomain, kMACaptionAppearanceDisplayTypeForcedOnly);
  }
  return true;
}

}  // namespace accessibility
}  // namespace uikit
}  // namespace shared
}  // namespace starboard
