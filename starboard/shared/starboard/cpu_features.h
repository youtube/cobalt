// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard CPU features API

// Provide helper classes, macros and functions useful to the implementations of
// Starboard CPU features API on all platforms

#ifndef STARBOARD_SHARED_STARBOARD_CPU_FEATURES_H_
#define STARBOARD_SHARED_STARBOARD_CPU_FEATURES_H_

#include "starboard/cpu_features.h"

namespace starboard {
namespace shared {

// Set the general features of SbCPUFeatures to be invalid
inline void SetGeneralFeaturesInvalid(SbCPUFeatures* features) {
  features->brand = "";
  features->icache_line_size = -1;
  features->dcache_line_size = -1;
  features->hwcap = 0;
  features->hwcap2 = 0;
}

// Set the features of SbCPUFeatures.x86 to be invalid
inline void SetX86FeaturesInvalid(SbCPUFeatures* features) {
  features->x86.vendor = "";
  features->x86.family = -1;
  features->x86.ext_family = -1;
  features->x86.model = -1;
  features->x86.ext_model = -1;
  features->x86.stepping = -1;
  features->x86.type = -1;
  features->x86.signature = -1;
}

// Set the features of SbCPUFeatures.arm to be invalid
inline void SetArmFeaturesInvalid(SbCPUFeatures* features) {
  features->arm.implementer = -1;
  features->arm.variant = -1;
  features->arm.revision = -1;
  features->arm.architecture_generation = -1;
  features->arm.part = -1;
}

}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_CPU_FEATURES_H_
