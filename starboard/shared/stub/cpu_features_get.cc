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

#include "starboard/cpu_features.h"

#include <string.h>

#if SB_API_VERSION >= 11

bool SbCPUFeaturesGet(SbCPUFeatures* features) {
  memset(features, 0, sizeof(*features));
  features->architecture = kSbCPUFeaturesArchitectureUnknown;
  features->brand = "";
  features->cache_size = -1;
  features->has_fpu = false;
  features->hwcap = 0;
  features->hwcap2 = 0;

  features->arm.implementer = -1;
  features->arm.variant = -1;
  features->arm.revision = -1;
  features->arm.architecture_generation = -1;
  features->arm.part = -1;

  features->x86.vendor = "";
  features->x86.family = -1;
  features->x86.ext_family = -1;
  features->x86.model = -1;
  features->x86.ext_model = -1;
  features->x86.stepping = -1;
  features->x86.type = -1;
  features->x86.signature = -1;

  return false;
}
#endif  // SB_API_VERSION >= 11
