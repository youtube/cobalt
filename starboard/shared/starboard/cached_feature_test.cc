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

#include "starboard/shared/starboard/cached_feature.h"

#include "starboard/shared/starboard/feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard::features {
namespace {

const SbFeature kCachedFeatureTestEnabled = {"CachedFeatureTestEnabled", true};
const SbFeature kCachedFeatureTestDisabled = {"CachedFeatureTestDisabled",
                                              false};

class CachedFeatureTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    SbFeature features[] = {
        kCachedFeatureTestEnabled,
        kCachedFeatureTestDisabled,
    };
    FeatureList::InitializeFeatureList(features, 2, nullptr, 0);
  }

  void SetUp() override { FeatureList::ClearAllFeaturesForTesting(); }

  void TearDown() override { FeatureList::ClearAllFeaturesForTesting(); }
};

TEST_F(CachedFeatureTest, BasicBehavior) {
  CachedFeature cached_enabled(kCachedFeatureTestEnabled);
  CachedFeature cached_disabled(kCachedFeatureTestDisabled);

  // 1. Initial check (should query FeatureList)
  EXPECT_TRUE(cached_enabled.IsEnabled());
  EXPECT_FALSE(cached_disabled.IsEnabled());

  // 2. Override the feature in FeatureList
  FeatureList::SetFeatureForTesting(kCachedFeatureTestEnabled, false);
  FeatureList::SetFeatureForTesting(kCachedFeatureTestDisabled, true);

  // 3. Check again (should return CACHED value, ignoring the override)
  EXPECT_TRUE(cached_enabled.IsEnabled());
  EXPECT_FALSE(cached_disabled.IsEnabled());

  // 4. Clear cache
  cached_enabled.ClearCacheForTesting();
  cached_disabled.ClearCacheForTesting();

  // 5. Check again (should query FeatureList again and get the NEW overridden
  // value)
  EXPECT_FALSE(cached_enabled.IsEnabled());
  EXPECT_TRUE(cached_disabled.IsEnabled());
}

}  // namespace
}  // namespace starboard::features
