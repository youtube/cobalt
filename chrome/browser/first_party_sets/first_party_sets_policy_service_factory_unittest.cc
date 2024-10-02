// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace first_party_sets {

class FirstPartySetsPolicyServiceFactoryTest : public testing::Test {
 public:
  FirstPartySetsPolicyServiceFactoryTest() = default;

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  void TearDown() override { profile_manager_.DeleteAllTestingProfiles(); }

  TestingProfileManager& profile_manager() { return profile_manager_; }

 private:
  content::BrowserTaskEnvironment env_;
  TestingProfileManager profile_manager_ =
      TestingProfileManager(TestingBrowserProcess::GetGlobal());
};

TEST_F(FirstPartySetsPolicyServiceFactoryTest,
       ServiceCreatedRegardlessIfPolicyEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kFirstPartySets);
  TestingProfile* disabled_profile =
      profile_manager().CreateTestingProfile("disabled");
  TestingProfile* enabled_profile =
      profile_manager().CreateTestingProfile("enabled");

  base::Value empty_lists = base::JSONReader::Read(R"(
             {
                "replacements": [],
                "additions": []
              }
            )")
                                .value();
  base::Value expected_policy = empty_lists.Clone();
  disabled_profile->GetPrefs()->SetBoolean(
      prefs::kPrivacySandboxFirstPartySetsEnabled, false);
  disabled_profile->GetPrefs()->SetDict(
      first_party_sets::kFirstPartySetsOverrides,
      std::move(empty_lists.Clone().GetDict()));
  enabled_profile->GetPrefs()->SetBoolean(
      prefs::kPrivacySandboxFirstPartySetsEnabled, true);
  enabled_profile->GetPrefs()->SetDict(
      first_party_sets::kFirstPartySetsOverrides,
      std::move(empty_lists.GetDict()));

  // Ensure that the Service creation isn't reliant on the enabled pref.
  EXPECT_NE(FirstPartySetsPolicyServiceFactory::GetForBrowserContext(
                disabled_profile->GetOriginalProfile()),
            nullptr);
  EXPECT_NE(FirstPartySetsPolicyServiceFactory::GetForBrowserContext(
                enabled_profile->GetOriginalProfile()),
            nullptr);
}

TEST_F(FirstPartySetsPolicyServiceFactoryTest,
       OffTheRecordProfile_SameServiceAsOriginalProfile) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kFirstPartySets);
  TestingProfile* profile =
      profile_manager().CreateTestingProfile("TestProfile");

  FirstPartySetsPolicyService* service =
      FirstPartySetsPolicyServiceFactory::GetForBrowserContext(
          profile->GetOriginalProfile());

  auto otr_profile_id = Profile::OTRProfileID::CreateUniqueForTesting();
  ASSERT_NE(service, nullptr);
  EXPECT_EQ(service,
            FirstPartySetsPolicyServiceFactory::GetForBrowserContext(
                profile->GetOffTheRecordProfile(otr_profile_id,
                                                /*create_if_needed=*/true)));
}

}  // namespace first_party_sets
