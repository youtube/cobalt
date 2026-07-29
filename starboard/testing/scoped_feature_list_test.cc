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

#include "starboard/testing/scoped_feature_list.h"

#include "starboard/shared/starboard/feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard::features {
namespace {

const SbFeature kTestFeatureEnabledByDefault = {"TestFeatureEnabledByDefault",
                                                true};
const SbFeature kTestFeatureDisabledByDefault = {"TestFeatureDisabledByDefault",
                                                 false};

class ScopedFeatureListTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    SbFeature features[] = {
        kTestFeatureEnabledByDefault,
        kTestFeatureDisabledByDefault,
    };
    FeatureList::InitializeFeatureList(features, 2, nullptr, 0);
  }

  void SetUp() override {
    // Ensure no leftovers from other tests.
    FeatureList::ClearAllFeaturesForTesting();
  }

  void TearDown() override { FeatureList::ClearAllFeaturesForTesting(); }
};

TEST_F(ScopedFeatureListTest, EnableFeature) {
  EXPECT_FALSE(FeatureList::IsEnabled(kTestFeatureDisabledByDefault));

  {
    ScopedFeatureList scoped_list;
    scoped_list.InitAndEnableFeature(kTestFeatureDisabledByDefault);
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeatureDisabledByDefault));
  }

  EXPECT_FALSE(FeatureList::IsEnabled(kTestFeatureDisabledByDefault));
}

TEST_F(ScopedFeatureListTest, DisableFeature) {
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeatureEnabledByDefault));

  {
    ScopedFeatureList scoped_list;
    scoped_list.InitAndDisableFeature(kTestFeatureEnabledByDefault);
    EXPECT_FALSE(FeatureList::IsEnabled(kTestFeatureEnabledByDefault));
  }

  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeatureEnabledByDefault));
}

TEST_F(ScopedFeatureListTest, MultipleFeatures) {
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeatureEnabledByDefault));
  EXPECT_FALSE(FeatureList::IsEnabled(kTestFeatureDisabledByDefault));

  {
    ScopedFeatureList scoped_list;
    scoped_list.InitAndDisableFeature(kTestFeatureEnabledByDefault);
    scoped_list.InitAndEnableFeature(kTestFeatureDisabledByDefault);

    EXPECT_FALSE(FeatureList::IsEnabled(kTestFeatureEnabledByDefault));
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeatureDisabledByDefault));
  }

  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeatureEnabledByDefault));
  EXPECT_FALSE(FeatureList::IsEnabled(kTestFeatureDisabledByDefault));
}

TEST_F(ScopedFeatureListTest, NestedDifferentFeatures) {
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeatureEnabledByDefault));
  EXPECT_FALSE(FeatureList::IsEnabled(kTestFeatureDisabledByDefault));

  {
    ScopedFeatureList outer;
    outer.InitAndDisableFeature(kTestFeatureEnabledByDefault);
    EXPECT_FALSE(FeatureList::IsEnabled(kTestFeatureEnabledByDefault));

    {
      ScopedFeatureList inner;
      inner.InitAndEnableFeature(kTestFeatureDisabledByDefault);

      EXPECT_FALSE(FeatureList::IsEnabled(kTestFeatureEnabledByDefault));
      EXPECT_TRUE(FeatureList::IsEnabled(kTestFeatureDisabledByDefault));
    }

    // Inner destructed, kTestFeatureDisabledByDefault should be restored.
    EXPECT_FALSE(FeatureList::IsEnabled(kTestFeatureEnabledByDefault));
    EXPECT_FALSE(FeatureList::IsEnabled(kTestFeatureDisabledByDefault));
  }

  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeatureEnabledByDefault));
  EXPECT_FALSE(FeatureList::IsEnabled(kTestFeatureDisabledByDefault));
}

TEST_F(ScopedFeatureListTest, NestedSameFeature) {
  EXPECT_FALSE(FeatureList::IsEnabled(kTestFeatureDisabledByDefault));

  {
    ScopedFeatureList outer;
    outer.InitAndEnableFeature(kTestFeatureDisabledByDefault);
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeatureDisabledByDefault));

    {
      ScopedFeatureList inner;
      inner.InitAndDisableFeature(kTestFeatureDisabledByDefault);
      EXPECT_FALSE(FeatureList::IsEnabled(kTestFeatureDisabledByDefault));
    }

    // Inner destructed, outer's override should be restored (enabled).
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeatureDisabledByDefault));
  }

  // Outer destructed, should be back to default (disabled).
  EXPECT_FALSE(FeatureList::IsEnabled(kTestFeatureDisabledByDefault));
}

TEST_F(ScopedFeatureListTest, NestedSameFeatureStartEnabled) {
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeatureEnabledByDefault));

  {
    ScopedFeatureList outer;
    outer.InitAndDisableFeature(kTestFeatureEnabledByDefault);
    EXPECT_FALSE(FeatureList::IsEnabled(kTestFeatureEnabledByDefault));

    {
      ScopedFeatureList inner;
      inner.InitAndEnableFeature(kTestFeatureEnabledByDefault);
      EXPECT_TRUE(FeatureList::IsEnabled(kTestFeatureEnabledByDefault));
    }

    // Inner destructed, outer's override should be restored (disabled).
    EXPECT_FALSE(FeatureList::IsEnabled(kTestFeatureEnabledByDefault));
  }

  // Outer destructed, should be back to default (enabled).
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeatureEnabledByDefault));
}

}  // namespace
}  // namespace starboard::features
