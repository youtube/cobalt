// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "starboard/drm.h"

#include "starboard/nplb/drm_helpers.h"
#include "starboard/string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

#if SB_API_VERSION >= SB_DRM_REFINEMENT_API_VERSION

TEST(SbDrmUpdateServerCertificateTest, SunnyDay) {
  // Ensure that |SbDrmUpdateServerCertificate| can be called over all key
  // systems.
  for (auto key_system : kKeySystems) {
    SbDrmSystem drm_system = CreateDummyDrmSystem(key_system);
    if (!SbDrmSystemIsValid(drm_system)) {
      continue;
    }
    const char kInvalidCertificate[] = "*Cobalt*";
    if (SbDrmIsServerCertificateUpdatable(drm_system)) {
      SbDrmUpdateServerCertificate(drm_system, kSbDrmTicketInvalid + 1,
                                   kInvalidCertificate,
                                   SbStringGetLength(kInvalidCertificate));
    }
    SbDrmDestroySystem(drm_system);
  }
}

#endif  // SB_API_VERSION >= SB_DRM_REFINEMENT_API_VERSION

}  // namespace
}  // namespace nplb
}  // namespace starboard
