// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/google_lens_step_eligibility_checker.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/lens/lens_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class GoogleLensStepEligibilityCheckerTest : public testing::Test {
 public:
  GoogleLensStepEligibilityCheckerTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlay, {{"google-dse-required", "false"}});
  }

  void SetUp() override { testing::Test::SetUp(); }

  void TearDown() override { testing::Test::TearDown(); }

  TestingProfile* profile() { return &profile_; }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(GoogleLensStepEligibilityCheckerTest, EligibleWhenAllConditionsMet) {
  GoogleLensStepEligibilityChecker checker;
  base::test::TestFuture<bool> future;
  checker.CheckEligibility(*profile(), future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(GoogleLensStepEligibilityCheckerTest, IneligibleWhenNonGoogleDSE) {
  feature_list_.Reset();
  feature_list_.InitAndEnableFeatureWithParameters(
      lens::features::kLensOverlay, {{"google-dse-required", "true"}});

  TemplateURLServiceFactoryTestUtil factory_util(profile());
  factory_util.VerifyLoad();

  TemplateURLData non_google_turl;
  non_google_turl.SetKeyword(u"non_google");
  non_google_turl.SetURL("https://www.example.com/?q={searchTerms}");
  factory_util.model()->SetUserSelectedDefaultSearchProvider(
      factory_util.model()->Add(
          std::make_unique<TemplateURL>(non_google_turl)));

  GoogleLensStepEligibilityChecker checker;
  base::test::TestFuture<bool> future;
  checker.CheckEligibility(*profile(), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(GoogleLensStepEligibilityCheckerTest,
       IneligibleWhenPermissionsAlreadyGranted) {
  GoogleLensStepEligibilityChecker checker;
  lens::GrantLensOverlayNeededPermissions(profile());

  base::test::TestFuture<bool> future;
  checker.CheckEligibility(*profile(), future.GetCallback());
  EXPECT_FALSE(future.Get());
}
