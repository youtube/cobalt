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

bool SbCPUFeaturesGet(SbCPUFeatures* features) {
  memset(features, 0, sizeof(*features));
  features->architecture = kSbCPUFeaturesArchitectureUnknown;
  features->brand = "";
  features->cache_size = -1;
  features->has_fpu = false;
  features->hwcap = 0;
  features->hwcap2 = 0;

  features->arm_features.implementer = -1;
  features->arm_features.variant = -1;
  features->arm_features.revision = -1;
  features->arm_features.architecture_generation = -1;
  features->arm_features.part = -1;

  features->X86_features.vendor = "";
  features->X86_features.family = -1;
  features->X86_features.ext_family = -1;
  features->X86_features.model = -1;
  features->X86_features.ext_model = -1;
  features->X86_features.stepping = -1;
  features->X86_features.type = -1;
  features->X86_features.signature = -1;

  return false;
}
