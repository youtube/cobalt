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

#include "starboard/extension/features.h"
#include "starboard/shared/starboard/feature_list.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(FeaturesExtensionTest, InitializeFeatures) {
  const StarboardExtensionFeaturesApi* extension_api =
      static_cast<const StarboardExtensionFeaturesApi*>(
          SbSystemGetExtension(kStarboardExtensionFeaturesName));
  ASSERT_NE(extension_api, nullptr);
  EXPECT_STREQ(extension_api->name, kStarboardExtensionFeaturesName);
  EXPECT_EQ(extension_api->version, 1u);
  ASSERT_NE(extension_api->InitializeStarboardFeatures, nullptr);

  SbFeature test_feature = {"AndroidExtensionTestFeature", true};

  SbFeatureParam test_param;
  test_param.feature_name = "AndroidExtensionTestFeature";
  test_param.param_name = "TestParam";
  test_param.type = SbFeatureParamTypeBool;
  test_param.value.bool_value = true;

  extension_api->InitializeStarboardFeatures(&test_feature, 1, &test_param, 1);

  EXPECT_TRUE(features::FeatureList::IsEnabled(test_feature));

  const features::SbFeatureParamExt<bool> test_param_ext(test_feature,
                                                         "TestParam");
  EXPECT_TRUE(test_param_ext.Get());
}

}  // namespace
}  // namespace starboard
