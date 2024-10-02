// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/user_population.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {
std::unique_ptr<PrefService> CreatePrefService() {
  auto pref_service =
      std::make_unique<sync_preferences::TestingPrefServiceSyncable>();

  safe_browsing::RegisterProfilePrefs(pref_service->registry());
  unified_consent::UnifiedConsentService::RegisterPrefs(
      pref_service->registry());

  return std::move(pref_service);
}

}  // namespace

TEST(GetUserPopulationTest, PopulatesPopulation) {
  base::test::TaskEnvironment task_environment;
  auto pref_service = CreatePrefService();

  SetSafeBrowsingState(pref_service.get(),
                       SafeBrowsingState::STANDARD_PROTECTION);
  ChromeUserPopulation population =
      GetUserPopulation(pref_service.get(), false, false, false, false, nullptr,
                        absl::optional<size_t>(), absl::optional<size_t>(),
                        absl::optional<size_t>());
  EXPECT_EQ(population.user_population(), ChromeUserPopulation::SAFE_BROWSING);

  SetSafeBrowsingState(pref_service.get(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  population =
      GetUserPopulation(pref_service.get(), false, false, false, false, nullptr,
                        absl::optional<size_t>(), absl::optional<size_t>(),
                        absl::optional<size_t>());

  EXPECT_EQ(population.user_population(),
            ChromeUserPopulation::ENHANCED_PROTECTION);

  SetSafeBrowsingState(pref_service.get(),
                       SafeBrowsingState::STANDARD_PROTECTION);
  SetExtendedReportingPrefForTests(pref_service.get(), true);
  population =
      GetUserPopulation(pref_service.get(), false, false, false, false, nullptr,
                        absl::optional<size_t>(), absl::optional<size_t>(),
                        absl::optional<size_t>());
  EXPECT_EQ(population.user_population(),
            ChromeUserPopulation::EXTENDED_REPORTING);
}

TEST(GetUserPopulationTest, PopulatesMBB) {
  base::test::TaskEnvironment task_environment;
  auto pref_service = CreatePrefService();

  pref_service->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  ChromeUserPopulation population =
      GetUserPopulation(pref_service.get(), false, false, false, false, nullptr,
                        absl::optional<size_t>(), absl::optional<size_t>(),
                        absl::optional<size_t>());
  EXPECT_FALSE(population.is_mbb_enabled());

  pref_service->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  population =
      GetUserPopulation(pref_service.get(), false, false, false, false, nullptr,
                        absl::optional<size_t>(), absl::optional<size_t>(),
                        absl::optional<size_t>());
  EXPECT_TRUE(population.is_mbb_enabled());
}

TEST(GetUserPopulationTest, PopulatesIncognito) {
  base::test::TaskEnvironment task_environment;
  auto pref_service = CreatePrefService();

  ChromeUserPopulation population =
      GetUserPopulation(pref_service.get(), /*is_incognito=*/false, false,
                        false, false, nullptr, absl::optional<size_t>(),
                        absl::optional<size_t>(), absl::optional<size_t>());
  EXPECT_FALSE(population.is_incognito());

  population =
      GetUserPopulation(pref_service.get(), /*is_incognito=*/true, false, false,
                        false, nullptr, absl::optional<size_t>(),
                        absl::optional<size_t>(), absl::optional<size_t>());
  EXPECT_TRUE(population.is_incognito());
}

TEST(GetUserPopulationTest, PopulatesSync) {
  base::test::TaskEnvironment task_environment;
  auto pref_service = CreatePrefService();

  ChromeUserPopulation population = GetUserPopulation(
      pref_service.get(), false, /*is_history_sync_enabled=*/true, false, false,
      nullptr, absl::optional<size_t>(), absl::optional<size_t>(),
      absl::optional<size_t>());
  EXPECT_TRUE(population.is_history_sync_enabled());

  population = GetUserPopulation(
      pref_service.get(), false, /*is_history_sync_enabled=*/false, false,
      false, nullptr, absl::optional<size_t>(), absl::optional<size_t>(),
      absl::optional<size_t>());
  EXPECT_FALSE(population.is_history_sync_enabled());
}

TEST(GetUserPopulationTest, PopulatesSignedIn) {
  base::test::TaskEnvironment task_environment;
  auto pref_service = CreatePrefService();

  ChromeUserPopulation population =
      GetUserPopulation(pref_service.get(), false, false, /*is_signed_in=*/true,
                        false, nullptr, absl::optional<size_t>(),
                        absl::optional<size_t>(), absl::optional<size_t>());
  EXPECT_TRUE(population.is_signed_in());

  population = GetUserPopulation(
      pref_service.get(), false, false, /*is_signed_in=*/false, false, nullptr,
      absl::optional<size_t>(), absl::optional<size_t>(),
      absl::optional<size_t>());
  EXPECT_FALSE(population.is_signed_in());
}

TEST(GetUserPopulationTest, PopulatesAdvancedProtection) {
  base::test::TaskEnvironment task_environment;
  auto pref_service = CreatePrefService();

  ChromeUserPopulation population = GetUserPopulation(
      pref_service.get(), false, false, false,
      /*is_under_advanced_protection=*/true, nullptr, absl::optional<size_t>(),
      absl::optional<size_t>(), absl::optional<size_t>());
  EXPECT_TRUE(population.is_under_advanced_protection());

  population = GetUserPopulation(
      pref_service.get(), false, false, false,
      /*is_under_advanced_protection=*/false, nullptr, absl::optional<size_t>(),
      absl::optional<size_t>(), absl::optional<size_t>());
  EXPECT_FALSE(population.is_under_advanced_protection());
}

TEST(GetUserPopulationTest, PopulatesUserAgent) {
  base::test::TaskEnvironment task_environment;
  auto pref_service = CreatePrefService();
  std::string user_agent =
      version_info::GetProductNameAndVersionForUserAgent() + "/" +
      version_info::GetOSType();
  ChromeUserPopulation population =
      GetUserPopulation(pref_service.get(), false, false, false, false, nullptr,
                        absl::optional<size_t>(), absl::optional<size_t>(),
                        absl::optional<size_t>());
  EXPECT_EQ(population.user_agent(), user_agent);
}

TEST(GetUserPopulationTest, PopulatesProfileRelatedFields) {
  base::test::TaskEnvironment task_environment;
  auto pref_service = CreatePrefService();
  ChromeUserPopulation population =
      GetUserPopulation(pref_service.get(), false, false, false, false, nullptr,
                        absl::optional<size_t>(), absl::optional<size_t>(),
                        absl::optional<size_t>());
  EXPECT_EQ(population.number_of_profiles(), 0);
  EXPECT_EQ(population.number_of_loaded_profiles(), 0);
  EXPECT_EQ(population.number_of_open_profiles(), 0);

  population = GetUserPopulation(
      pref_service.get(), false, false, false, false, nullptr,
      /*num_profiles=*/3, /*num_loaded_profiles=*/2, /*num_open_profiles=*/1);
  EXPECT_EQ(population.number_of_profiles(), 3);
  EXPECT_EQ(population.number_of_loaded_profiles(), 2);
  EXPECT_EQ(population.number_of_open_profiles(), 1);
}

}  // namespace safe_browsing
