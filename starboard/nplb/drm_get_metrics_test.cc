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

#include "starboard/drm.h"

#include "starboard/common/string.h"
#include "starboard/nplb/drm_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(SbDrmGetMetricsTest, SunnyDay) {
  for (auto key_system : kKeySystems) {
    SbDrmSystem drm_system = CreateDummyDrmSystem(key_system);
    if (!SbDrmSystemIsValid(drm_system)) {
      continue;
    }
    int size = -1;
    const void* metrics = SbDrmGetMetrics(drm_system, &size);
    if (size > 0) {
      ASSERT_TRUE(metrics);
    }
    if (metrics) {
      ASSERT_GE(size, 0);
    }
    SbDrmDestroySystem(drm_system);
  }
}

}  // namespace
}  // namespace starboard
