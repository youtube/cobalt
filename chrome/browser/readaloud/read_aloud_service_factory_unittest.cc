// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/readaloud/read_aloud_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"

namespace readaloud {

class ReadAloudServiceFactoryTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ReadAloudServiceFactoryTest, FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kReadAloudNative);

  TestingProfile profile;
  EXPECT_EQ(nullptr, ReadAloudServiceFactory::GetForProfile(&profile));
}

TEST_F(ReadAloudServiceFactoryTest, FeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kReadAloudNative);

  TestingProfile profile;
  EXPECT_NE(nullptr, ReadAloudServiceFactory::GetForProfile(&profile));
}

TEST_F(ReadAloudServiceFactoryTest, OffTheRecordProfile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kReadAloudNative);

  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);

  EXPECT_EQ(nullptr, ReadAloudServiceFactory::GetForProfile(otr_profile));
}

}  // namespace readaloud
