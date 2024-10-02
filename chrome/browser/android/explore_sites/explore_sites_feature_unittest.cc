// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_feature.h"

#include <map>
#include <string>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {
namespace android {
namespace explore_sites {

TEST(ExploreSitesFeatureTest, ExploreSitesEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kExploreSites);
  EXPECT_EQ(ExploreSitesVariation::ENABLED, GetExploreSitesVariation());
}

TEST(ExploreSitesFeatureTest, ExploreSitesDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kExploreSites);
  EXPECT_EQ(ExploreSitesVariation::DISABLED, GetExploreSitesVariation());
}

TEST(ExploreSitesFeatureTest, ExploreSitesEnabledWithDenseTitleBottom) {
  std::map<std::string, std::string> parameters;
  parameters[kExploreSitesDenseVariationParameterName] =
      kExploreSitesDenseVariationDenseTitleBottom;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(kExploreSites,
                                                         parameters);
  EXPECT_EQ(DenseVariation::DENSE_TITLE_BOTTOM, GetDenseVariation());
}

TEST(ExploreSitesFeatureTest, ExploreSitesEnabledWithDenseTitleRight) {
  std::map<std::string, std::string> parameters;
  parameters[kExploreSitesDenseVariationParameterName] =
      kExploreSitesDenseVariationDenseTitleRight;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(kExploreSites,
                                                         parameters);
  EXPECT_EQ(DenseVariation::DENSE_TITLE_RIGHT, GetDenseVariation());
}

TEST(ExploreSitesFeatureTest, ExploreSitesDenseVariationOriginal) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kExploreSites);
  EXPECT_EQ(DenseVariation::ORIGINAL, GetDenseVariation());
}

}  // namespace explore_sites
}  // namespace android
}  // namespace chrome
