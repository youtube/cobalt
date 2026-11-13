// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

// Tests that the platform's values for the constants declared in
// configuration_constants.h meet the requirements Cobalt places on them.

#include "starboard/configuration_constants.h"

#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

// The minimum value allowed for |kSbMaxSystemPathCacheDirectorySize|. A cache
// budget of at least 24 MiB is a device certification requirement.
constexpr uint32_t kMinCacheDirectorySize = 24 * 1024 * 1024;

TEST(SbConfigurationConstantsTest,
     MaxSystemPathCacheDirectorySizeIsAtLeast24MiB) {
  EXPECT_GE(kSbMaxSystemPathCacheDirectorySize, kMinCacheDirectorySize)
      << "kSbMaxSystemPathCacheDirectorySize is "
      << kSbMaxSystemPathCacheDirectorySize
      << " bytes. The platform must allow the cache directory "
      << "(kSbSystemPathCacheDirectory) to use at least 24 MiB ("
      << kMinCacheDirectorySize << " bytes). Set the constant to 24 MiB or "
      << "more in the platform's configuration_constants.cc.";
}

}  // namespace
}  // namespace nplb
