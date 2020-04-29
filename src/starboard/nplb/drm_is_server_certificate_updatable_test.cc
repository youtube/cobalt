// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbDrmIsServerCertificateUpdatableTest, SunnyDay) {
  // Ensure that |SbDrmIsServerCertificateUpdatable| can be called over all key
  // systems.
  for (auto key_system : kKeySystems) {
    SbDrmSystem drm_system = CreateDummyDrmSystem(key_system);
    if (!SbDrmSystemIsValid(drm_system)) {
      continue;
    }
    SbDrmIsServerCertificateUpdatable(drm_system);
    SbDrmDestroySystem(drm_system);
  }
}

TEST(SbDrmIsServerCertificateUpdatableTest, Consistency) {
  // Ensure that the function returns the same results for the same key system
  // when being called for more than once.
  for (auto key_system : kKeySystems) {
    SbDrmSystem drm_system = CreateDummyDrmSystem(key_system);
    if (!SbDrmSystemIsValid(drm_system)) {
      continue;
    }
    bool updatable = SbDrmIsServerCertificateUpdatable(drm_system);
    EXPECT_EQ(SbDrmIsServerCertificateUpdatable(drm_system), updatable);
    SbDrmDestroySystem(drm_system);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
