// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/features.h"
#include "starboard/shared/starboard/feature_list.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard::android::shared {

TEST(StarboardFeatureTest, CanUseFeature) {
  EXPECT_TRUE(features::FeatureList::IsEnabled(features::kFeatureFoo));
  EXPECT_FALSE(features::FeatureList::IsEnabled(features::kFeatureBar));
}

TEST(StarboardFeatureTest, CanUseFeatureParam) {
  EXPECT_EQ(10, starboard::features::kFeatureParamBoo.Get());
}

}  // namespace starboard::android::shared
