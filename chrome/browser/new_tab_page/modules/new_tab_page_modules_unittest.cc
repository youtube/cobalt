// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/new_tab_page/modules/modules_switches.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"
#include "chrome/grit/generated_resources.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp {

const std::vector<base::test::FeatureRef>& kAllModuleFeatures = {
    ntp_features::kNtpChromeCartModule,
    ntp_features::kNtpDriveModule,
    ntp_features::kNtpFeedModule,
    ntp_features::kNtpHistoryClustersModule,
    ntp_features::kNtpRecipeTasksModule,
};

std::vector<base::test::FeatureRef> ComputeDisabledFeaturesList(
    const std::vector<base::test::FeatureRef>& features,
    const std::vector<base::test::FeatureRef>& enabled_features) {
  std::vector<base::test::FeatureRef> disabled_features;
  std::copy_if(features.begin(), features.end(),
               std::back_inserter(disabled_features),
               [&enabled_features](base::test::FeatureRef feature_to_copy) {
                 return !base::Contains(enabled_features, feature_to_copy);
               });
  return disabled_features;
}

TEST(NewTabPageModulesTest, MakeModuleIdNames_SingleModuleEnabled) {
  const std::vector<base::test::FeatureRef>& some_module_features = {
      ntp_features::kNtpRecipeTasksModule, ntp_features::kNtpFeedModule,
      ntp_features::kNtpHistoryClustersModule};
  for (auto& feature : some_module_features) {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{feature},
        /*disabled_features=*/ComputeDisabledFeaturesList(some_module_features,
                                                          {feature}));

    const std::vector<std::pair<const std::string, int>> module_id_names =
        MakeModuleIdNames(false);
    ASSERT_EQ(1u, module_id_names.size());
  }
}

TEST(NewTabPageModulesTest, MakeModuleIdNames_WithDriveModule) {
  base::test::ScopedFeatureList features;
  const std::vector<base::test::FeatureRef>& enabled_features = {
      ntp_features::kNtpRecipeTasksModule, ntp_features::kNtpDriveModule};
  features.InitWithFeatures(
      /*enabled_features=*/enabled_features,
      /*disabled_features=*/ComputeDisabledFeaturesList(kAllModuleFeatures,
                                                        enabled_features));

  const std::vector<std::pair<const std::string, int>> module_id_names =
      MakeModuleIdNames(true);
  ASSERT_EQ(2u, module_id_names.size());
}

#if !defined(OFFICIAL_BUILD)
TEST(NewTabPageModulesTest, MakeModuleIdNames_DummyModules) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpDummyModules},
      /*disabled_features=*/kAllModuleFeatures);

  const std::vector<std::pair<const std::string, int>> module_id_names =
      MakeModuleIdNames(false);
  ASSERT_EQ(1u, module_id_names.size());
}
#endif

const char kSampleUserEmail[] = "user@gmail.com";
const std::vector<std::pair<const std::string, int>> kSampleModules = {
    {"recipe_tasks", IDS_NTP_MODULES_RECIPE_TASKS_SENTENCE}};

TEST(NewTabPageModulesTest, HasModulesEnabled_SignedInAccount) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.SetCookieAccounts(
      {{kSampleUserEmail, signin::GetTestGaiaIdForEmail(kSampleUserEmail)}});
  ASSERT_TRUE(
      HasModulesEnabled(kSampleModules, identity_test_env.identity_manager()));
}

TEST(NewTabPageModulesTest,
     HasModulesEnabled_SignedInAccountNtpModulesLoadFlag) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpModulesLoad},
      /*disabled_features=*/{});

  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.SetCookieAccounts(
      {{kSampleUserEmail, signin::GetTestGaiaIdForEmail(kSampleUserEmail)}});
  ASSERT_FALSE(
      HasModulesEnabled(kSampleModules, identity_test_env.identity_manager()));
}

TEST(NewTabPageModulesTest, HasModulesEnabled_NoSignedInAccount) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;
  ASSERT_FALSE(
      HasModulesEnabled(kSampleModules, identity_test_env.identity_manager()));
}

TEST(NewTabPageModulesTest,
     HasModulesEnabled_NoSignedInAccountSignedOutModulesSwitch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kSignedOutNtpModulesSwitch);
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;
  ASSERT_TRUE(
      HasModulesEnabled(kSampleModules, identity_test_env.identity_manager()));
}

TEST(NewTabPageModulesTest, ShowChromeCart_WithoutChromeCartInHistoryModule) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpChromeCartModule,
                            ntp_features::kNtpHistoryClustersModule},
      /*disabled_features=*/{
          ntp_features::kNtpChromeCartInHistoryClusterModule});

  const std::vector<std::pair<const std::string, int>> module_id_names =
      MakeModuleIdNames(false);
  std::vector<std::string> module_names;
  for (auto pair : module_id_names) {
    module_names.emplace_back(pair.first);
  }
  ASSERT_FALSE(std::find(module_names.begin(), module_names.end(),
                         "chrome_cart") == module_names.end());
}

TEST(NewTabPageModulesTest, NoChromeCart_WithChromeCartInHistoryModule) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpChromeCartModule,
                            ntp_features::kNtpHistoryClustersModule,
                            ntp_features::kNtpChromeCartInHistoryClusterModule},
      /*disabled_features=*/{});

  const std::vector<std::pair<const std::string, int>> module_id_names =
      MakeModuleIdNames(false);
  std::vector<std::string> module_names;
  for (auto pair : module_id_names) {
    module_names.emplace_back(pair.first);
  }
  ASSERT_TRUE(std::find(module_names.begin(), module_names.end(),
                        "chrome_cart") == module_names.end());
}

TEST(NewTabPageModulesTest,
     ShowChromeCart_WithChromeCartInHistoryModuleCoexist) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpChromeCartModule,
                            ntp_features::kNtpHistoryClustersModule,
                            ntp_features::kNtpChromeCartInHistoryClusterModule,
                            ntp_features::kNtpChromeCartHistoryClusterCoexist},
      /*disabled_features=*/{});

  const std::vector<std::pair<const std::string, int>> module_id_names =
      MakeModuleIdNames(false);
  std::vector<std::string> module_names;
  for (auto pair : module_id_names) {
    module_names.emplace_back(pair.first);
  }
  ASSERT_FALSE(std::find(module_names.begin(), module_names.end(),
                         "chrome_cart") == module_names.end());
}
}  // namespace ntp
