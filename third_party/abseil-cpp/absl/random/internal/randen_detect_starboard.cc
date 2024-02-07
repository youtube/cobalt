// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// HERMETIC NOTE: The randen_hwaes target must not introduce duplicate
// symbols from arbitrary system and other headers, since it may be built
// with different flags from other targets, using different levels of
// optimization, potentially introducing ODR violations.

#include "absl/random/internal/randen_detect.h"

#include <cstdint>
#include <cstring>

#include "absl/random/internal/platform.h"
#include "starboard/cpu_features.h"

#if !defined(STARBOARD)
#error This file is a specialization for Starboard only.
#endif

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace random_internal {

bool CPUSupportsRandenHwAes() {
  SbCPUFeatures features;
  bool result = SbCPUFeaturesGet(&features);
  if (!result) {
    return false;
  }
  if (features.architecture == kSbCPUFeaturesArchitectureArm ||
      features.architecture == kSbCPUFeaturesArchitectureArm64) {
    return features.arm.has_neon && features.arm.has_aes;
  }
  if (features.architecture == kSbCPUFeaturesArchitectureX86 ||
      features.architecture == kSbCPUFeaturesArchitectureX86_64) {
    return features.x86.has_aesni;
  }
  return false;
}

}  // namespace random_internal
ABSL_NAMESPACE_END
}  // namespace absl
