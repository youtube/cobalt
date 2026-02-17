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

#include "cobalt/browser/experiments/experiment_config_manager.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cobalt/browser/constants/cobalt_experiment_names.h"
#include "cobalt/browser/features.h"
#include "cobalt/version.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

class ExperimentConfigManagerTest : public testing::Test {
 public:
  ExperimentConfigManagerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_->registry()->RegisterIntegerPref(
        variations::prefs::kVariationsCrashStreak, 0);
    pref_service_->registry()->RegisterStringPref(
        kExperimentConfigActiveConfigData, std::string());
    pref_service_->registry()->RegisterDictionaryPref(
        kExperimentConfigFeatures);
    pref_service_->registry()->RegisterDictionaryPref(
        kExperimentConfigFeatureParams);
    pref_service_->registry()->RegisterStringPref(kExperimentConfigMinVersion,
                                                  std::string());
    pref_service_->registry()->RegisterStringPref(kSafeConfigActiveConfigData,
                                                  std::string());
    pref_service_->registry()->RegisterDictionaryPref(kSafeConfigFeatures);
    pref_service_->registry()->RegisterDictionaryPref(kSafeConfigFeatureParams);
    pref_service_->registry()->RegisterStringPref(kSafeConfigMinVersion,
                                                  std::string());
    pref_service_->registry()->RegisterDictionaryPref(kFinchParameters);
    pref_service_->registry()->RegisterTimePref(
        variations::prefs::kVariationsLastFetchTime, base::Time());
    metrics_pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    metrics_pref_service_->registry()->RegisterIntegerPref(
        variations::prefs::kVariationsCrashStreak, 0);

    experiment_config_manager_ = std::make_unique<ExperimentConfigManager>(
        pref_service_.get(), metrics_pref_service_.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  // This pref_service provides the finch experiment config.
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  // This metrics_pref_service manages the current state of metrics.
  std::unique_ptr<TestingPrefServiceSimple> metrics_pref_service_;
  std::unique_ptr<ExperimentConfigManager> experiment_config_manager_;
  base::HistogramTester histogram_tester_;
};

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsRegularWhenNotExpired) {
  base::Value::Dict feature_map;
  feature_map.Set(features::kExperimentConfigExpiration.name, true);
  pref_service_->SetDict(kExperimentConfigFeatures, std::move(feature_map));

  base::Value::Dict finch_params;
  finch_params.Set("experiment_expiration_threshold_days", 30);
  pref_service_->SetDict(kFinchParameters, std::move(finch_params));

  pref_service_->SetTime(variations::prefs::kVariationsLastFetchTime,
                         base::Time::Now() - base::Days(10));

  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kRegularConfig);
  histogram_tester_.ExpectUniqueSample("Cobalt.Finch.ConfigState", 0, 1);
  histogram_tester_.ExpectUniqueSample("Cobalt.Finch.ConfigAgeInDays", 10, 1);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsEmptyWhenExpired) {
  base::Value::Dict feature_map;
  feature_map.Set(features::kExperimentConfigExpiration.name, true);
  pref_service_->SetDict(kExperimentConfigFeatures, std::move(feature_map));

  base::Value::Dict finch_params;
  finch_params.Set("experiment_expiration_threshold_days", 30);
  pref_service_->SetDict(kFinchParameters, std::move(finch_params));

  pref_service_->SetTime(variations::prefs::kVariationsLastFetchTime,
                         base::Time::Now() - base::Days(31));

  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kEmptyConfig);
  histogram_tester_.ExpectUniqueSample("Cobalt.Finch.ConfigState", 1, 1);
  histogram_tester_.ExpectUniqueSample("Cobalt.Finch.ConfigAgeInDays", 31, 1);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsRegularWhenExpiredButFeatureDisabled) {
  base::Value::Dict feature_map;
  feature_map.Set(features::kExperimentConfigExpiration.name, false);
  pref_service_->SetDict(kExperimentConfigFeatures, std::move(feature_map));

  base::Value::Dict finch_params;
  finch_params.Set("experiment_expiration_threshold_days", 30);
  pref_service_->SetDict(kFinchParameters, std::move(finch_params));

  pref_service_->SetTime(variations::prefs::kVariationsLastFetchTime,
                         base::Time::Now() - base::Days(31));

  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kRegularConfig);
  // UMA is logged unconditionally, and the state should be kExpired because
  // the config is, in fact, expired.
  histogram_tester_.ExpectUniqueSample("Cobalt.Finch.ConfigState", 1, 1);
  histogram_tester_.ExpectUniqueSample("Cobalt.Finch.ConfigAgeInDays", 31, 1);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsRegularWhenAgeEqualsThreshold) {
  base::Value::Dict feature_map;
  feature_map.Set(features::kExperimentConfigExpiration.name, true);
  pref_service_->SetDict(kExperimentConfigFeatures, std::move(feature_map));

  base::Value::Dict finch_params;
  finch_params.Set("experiment_expiration_threshold_days", 30);
  pref_service_->SetDict(kFinchParameters, std::move(finch_params));

  pref_service_->SetTime(variations::prefs::kVariationsLastFetchTime,
                         base::Time::Now() - base::Days(30));

  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kRegularConfig);
  histogram_tester_.ExpectUniqueSample("Cobalt.Finch.ConfigState", 0, 1);
  histogram_tester_.ExpectUniqueSample("Cobalt.Finch.ConfigAgeInDays", 30, 1);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsEmptyForZeroDayThreshold) {
  base::Value::Dict feature_map;
  feature_map.Set(features::kExperimentConfigExpiration.name, true);
  pref_service_->SetDict(kExperimentConfigFeatures, std::move(feature_map));

  base::Value::Dict finch_params;
  finch_params.Set("experiment_expiration_threshold_days", 0);
  pref_service_->SetDict(kFinchParameters, std::move(finch_params));

  pref_service_->SetTime(variations::prefs::kVariationsLastFetchTime,
                         base::Time::Now() - base::Days(1));

  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kEmptyConfig);
  histogram_tester_.ExpectUniqueSample("Cobalt.Finch.ConfigState", 1, 1);
  histogram_tester_.ExpectUniqueSample("Cobalt.Finch.ConfigAgeInDays", 1, 1);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsRegularWhenTimestampIsMissing) {
  base::Value::Dict feature_map;
  feature_map.Set(features::kExperimentConfigExpiration.name, true);
  pref_service_->SetDict(kExperimentConfigFeatures, std::move(feature_map));

  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kRegularConfig);
  histogram_tester_.ExpectUniqueSample("Cobalt.Finch.ConfigState", 2, 1);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeUsesDefaultThresholdWhenMissing) {
  base::Value::Dict feature_map;
  feature_map.Set(features::kExperimentConfigExpiration.name, true);
  pref_service_->SetDict(kExperimentConfigFeatures, std::move(feature_map));

  // Set age to be less than the default of 30.
  pref_service_->SetTime(variations::prefs::kVariationsLastFetchTime,
                         base::Time::Now() - base::Days(20));

  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kRegularConfig);
  histogram_tester_.ExpectUniqueSample("Cobalt.Finch.ConfigState", 0, 1);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeUsesDefaultThresholdWhenMalformed) {
  base::Value::Dict feature_map;
  feature_map.Set(features::kExperimentConfigExpiration.name, true);
  pref_service_->SetDict(kExperimentConfigFeatures, std::move(feature_map));

  base::Value::Dict finch_params;
  finch_params.Set("experiment_expiration_threshold_days", "thirty");
  pref_service_->SetDict(kFinchParameters, std::move(finch_params));

  // Set age to be less than the default of 30.
  pref_service_->SetTime(variations::prefs::kVariationsLastFetchTime,
                         base::Time::Now() - base::Days(20));

  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kRegularConfig);
  histogram_tester_.ExpectUniqueSample("Cobalt.Finch.ConfigState", 0, 1);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsRegularWithNegativeThreshold) {
  base::Value::Dict feature_map;
  feature_map.Set(features::kExperimentConfigExpiration.name, true);
  pref_service_->SetDict(kExperimentConfigFeatures, std::move(feature_map));

  base::Value::Dict finch_params;
  finch_params.Set("experiment_expiration_threshold_days", -5);
  pref_service_->SetDict(kFinchParameters, std::move(finch_params));

  pref_service_->SetTime(variations::prefs::kVariationsLastFetchTime,
                         base::Time::Now() - base::Days(100));

  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kRegularConfig);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsRegularWithMalformedFeatureFlag) {
  base::Value::Dict feature_map;
  feature_map.Set(features::kExperimentConfigExpiration.name, "true");
  pref_service_->SetDict(kExperimentConfigFeatures, std::move(feature_map));

  pref_service_->SetTime(variations::prefs::kVariationsLastFetchTime,
                         base::Time::Now() - base::Days(100));

  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kRegularConfig);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsSafeWhenCrashStreakHighAndNotExpired) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    kDefaultCrashStreakSafeConfigThreshold);

  base::Value::Dict feature_map;
  feature_map.Set(features::kExperimentConfigExpiration.name, true);
  // Enable the feature in the SAFE config.
  pref_service_->SetDict(kSafeConfigFeatures, std::move(feature_map));

  pref_service_->SetTime(variations::prefs::kVariationsLastFetchTime,
                         base::Time::Now() - base::Days(5));

  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kSafeConfig);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsEmptyWhenSafeConfigIsExpired) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    kDefaultCrashStreakSafeConfigThreshold);

  base::Value::Dict feature_map;
  feature_map.Set(features::kExperimentConfigExpiration.name, true);
  pref_service_->SetDict(kSafeConfigFeatures, std::move(feature_map));

  base::Value::Dict finch_params;
  finch_params.Set("experiment_expiration_threshold_days", 30);
  pref_service_->SetDict(kFinchParameters, std::move(finch_params));

  pref_service_->SetTime(variations::prefs::kVariationsLastFetchTime,
                         base::Time::Now() - base::Days(31));

  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kEmptyConfig);
  histogram_tester_.ExpectUniqueSample("Cobalt.Finch.ConfigState", 1, 1);
  histogram_tester_.ExpectUniqueSample("Cobalt.Finch.ConfigAgeInDays", 31, 1);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsEmptyWhenCrashStreakVeryHigh) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    kDefaultCrashStreakEmptyConfigThreshold);

  // Set up a non-expired config.
  pref_service_->SetTime(variations::prefs::kVariationsLastFetchTime,
                         base::Time::Now() - base::Days(1));

  // The crash streak should take precedence over the expiration check.
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kEmptyConfig);
}

TEST_F(ExperimentConfigManagerTest, GetExperimentConfigTypeReturnsRegular) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    0);
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kRegularConfig);
}

TEST_F(ExperimentConfigManagerTest, GetExperimentConfigTypeReturnsSafe) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    kDefaultCrashStreakSafeConfigThreshold);
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kSafeConfig);
}

TEST_F(ExperimentConfigManagerTest, GetExperimentConfigTypeReturnsEmpty) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    kDefaultCrashStreakEmptyConfigThreshold);
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kEmptyConfig);
}

TEST_F(ExperimentConfigManagerTest, StoreSafeConfigIsNoOpForSafeConfig) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    kDefaultCrashStreakSafeConfigThreshold);

  experiment_config_manager_->StoreSafeConfig();
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(
      experiment_config_manager_->has_called_store_safe_config_for_testing());
  EXPECT_TRUE(pref_service_->GetDict(kSafeConfigFeatures).empty());
}

TEST_F(ExperimentConfigManagerTest,
       StoreSafeConfigIsNoOpWhenRegularConfigIsExpired) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    0);

  base::Value::Dict feature_map;
  feature_map.Set(features::kExperimentConfigExpiration.name, true);
  pref_service_->SetDict(kExperimentConfigFeatures, std::move(feature_map));

  base::Value::Dict finch_params;
  finch_params.Set("experiment_expiration_threshold_days", 30);
  pref_service_->SetDict(kFinchParameters, std::move(finch_params));

  pref_service_->SetTime(variations::prefs::kVariationsLastFetchTime,
                         base::Time::Now() - base::Days(31));

  experiment_config_manager_->StoreSafeConfig();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(pref_service_->GetDict(kSafeConfigFeatures).empty());
}

TEST_F(ExperimentConfigManagerTest, StoreSafeConfigIsNoOpForEmptyConfig) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    kDefaultCrashStreakEmptyConfigThreshold);

  experiment_config_manager_->StoreSafeConfig();
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(
      experiment_config_manager_->has_called_store_safe_config_for_testing());
  EXPECT_TRUE(pref_service_->GetDict(kSafeConfigFeatures).empty());
}

TEST_F(ExperimentConfigManagerTest, StoreSafeConfigIsOnlyCalledOnce) {
  base::Value::Dict initial_features;
  initial_features.Set("feature1", true);
  pref_service_->SetDict(kExperimentConfigFeatures, initial_features.Clone());
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    0);

  // First call should store the initial config.
  experiment_config_manager_->StoreSafeConfig();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(
      experiment_config_manager_->has_called_store_safe_config_for_testing());
  EXPECT_EQ(pref_service_->GetDict(kSafeConfigFeatures), initial_features);

  // Modify the regular config.
  base::Value::Dict updated_features;
  updated_features.Set("feature2", false);
  pref_service_->SetDict(kExperimentConfigFeatures, updated_features.Clone());

  // Second call should be a no-op.
  experiment_config_manager_->StoreSafeConfig();
  task_environment_.RunUntilIdle();

  // The safe config should remain unchanged.
  EXPECT_EQ(pref_service_->GetDict(kSafeConfigFeatures), initial_features);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeIgnoresCrashStreakInExperimentPrefs) {
  // Set a high crash streak in the experiment prefs (which should be ignored).
  pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                            kDefaultCrashStreakSafeConfigThreshold);

  // Set a safe crash streak in the metrics prefs (which should be used).
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    0);

  // Expect regular config because the metrics crash streak is 0.
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kRegularConfig);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsEmptyOnRegularConfigRollback) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    0);
  pref_service_->SetString(kExperimentConfigMinVersion, "99.android.0");
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kEmptyConfig);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsRegularWhenVersionMatches) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    0);
  pref_service_->SetString(kExperimentConfigMinVersion, COBALT_VERSION);
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kRegularConfig);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsRegularWhenMinVersionIsOlder) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    0);
  pref_service_->SetString(kExperimentConfigMinVersion, "0.lts.0");
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kRegularConfig);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsEmptyOnSafeConfigRollback) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    kDefaultCrashStreakSafeConfigThreshold);
  pref_service_->SetString(kSafeConfigMinVersion, "99.lts.0");
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kEmptyConfig);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsSafeWhenVersionMatches) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    kDefaultCrashStreakSafeConfigThreshold);
  pref_service_->SetString(kSafeConfigMinVersion, COBALT_VERSION);
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kSafeConfig);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsSafeWhenVersionIsOlder) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    kDefaultCrashStreakSafeConfigThreshold);
  pref_service_->SetString(kSafeConfigMinVersion, "0.lts.0");
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kSafeConfig);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsRegularWhenMinVersionIsEmpty) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    0);
  pref_service_->SetString(kExperimentConfigMinVersion, "");
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kRegularConfig);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsSafeWhenMinVersionIsEmpty) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    kDefaultCrashStreakSafeConfigThreshold);
  pref_service_->SetString(kSafeConfigMinVersion, "");
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kSafeConfig);
}

namespace {
// Helper function to construct a modified version string for testing.
std::string GetModifiedVersionString(int major_offset, int minor_offset) {
  std::vector<std::string> parts = base::SplitString(
      COBALT_VERSION, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() != 3) {
    return "99.99.99";  // Return a high version to ensure test fails loudly.
  }

  int major, minor;
  base::StringToInt(parts[0], &major);
  base::StringToInt(parts[2], &minor);

  return base::StringPrintf("%d.%s.%d", major + major_offset, parts[1].c_str(),
                            minor + minor_offset);
}
}  // namespace

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsEmptyOnMajorVersionRollback) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    0);
  // Recorded version is one major version ahead of the current version.
  std::string future_version = GetModifiedVersionString(1, 0);
  pref_service_->SetString(kExperimentConfigMinVersion, future_version);
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kEmptyConfig);
  histogram_tester_.ExpectUniqueSample("Cobalt.Finch.RollbackDetected", true,
                                       1);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsEmptyOnMinorVersionRollback) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    0);
  // Recorded version is one minor version ahead of the current version.
  std::string future_version = GetModifiedVersionString(0, 1);
  pref_service_->SetString(kExperimentConfigMinVersion, future_version);

  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kEmptyConfig);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsRegularWhenVersionIsOlder) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    0);
  // Recorded version is one major version behind the current version.
  std::string past_version = GetModifiedVersionString(-1, 0);
  pref_service_->SetString(kExperimentConfigMinVersion, past_version);
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kRegularConfig);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsRegularOnInvalidVersionFormat) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    0);
  pref_service_->SetString(kExperimentConfigMinVersion, "invalid-version");
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kRegularConfig);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeReturnsRegularWhenMinVersionHasNumericPurpose) {
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    0);
  // The middle component "123" should be ignored.
  pref_service_->SetString(kExperimentConfigMinVersion, "0.123.0");
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kRegularConfig);
}

TEST_F(ExperimentConfigManagerTest, CompareVersions) {
  using VersionComparisonResult =
      ExperimentConfigManager::VersionComparisonResult;

  // Test major version
  EXPECT_EQ(VersionComparisonResult::kGreaterThan,
            ExperimentConfigManager::CompareVersions("10.lts.0", "9.lts.0"));
  histogram_tester_.ExpectUniqueSample("Cobalt.Finch.VersionComparisonIsValid",
                                       true, 1);
  EXPECT_EQ(VersionComparisonResult::kLessThanOrEqual,
            ExperimentConfigManager::CompareVersions("9.lts.0", "10.lts.0"));
  histogram_tester_.ExpectUniqueSample("Cobalt.Finch.VersionComparisonIsValid",
                                       true, 2);

  // Test minor version
  EXPECT_EQ(VersionComparisonResult::kGreaterThan,
            ExperimentConfigManager::CompareVersions("9.lts.1", "9.lts.0"));
  EXPECT_EQ(VersionComparisonResult::kLessThanOrEqual,
            ExperimentConfigManager::CompareVersions("9.lts.0", "9.lts.1"));

  // Test equal versions
  EXPECT_EQ(VersionComparisonResult::kLessThanOrEqual,
            ExperimentConfigManager::CompareVersions("9.lts.0", "9.lts.0"));

  // Test different purpose strings
  EXPECT_EQ(
      VersionComparisonResult::kGreaterThan,
      ExperimentConfigManager::CompareVersions("10.android.0", "9.lts.0"));
  EXPECT_EQ(VersionComparisonResult::kLessThanOrEqual,
            ExperimentConfigManager::CompareVersions("9.android.0", "9.lts.1"));

  // Test invalid formats
  EXPECT_EQ(VersionComparisonResult::kInvalidFormat,
            ExperimentConfigManager::CompareVersions("invalid", "9.lts.0"));
  histogram_tester_.ExpectBucketCount("Cobalt.Finch.VersionComparisonIsValid",
                                      false, 1);
  EXPECT_EQ(VersionComparisonResult::kInvalidFormat,
            ExperimentConfigManager::CompareVersions("9.lts.0", "invalid"));
  histogram_tester_.ExpectBucketCount("Cobalt.Finch.VersionComparisonIsValid",
                                      false, 2);
  EXPECT_EQ(VersionComparisonResult::kInvalidFormat,
            ExperimentConfigManager::CompareVersions("9.0", "9.lts.0"));
  histogram_tester_.ExpectBucketCount("Cobalt.Finch.VersionComparisonIsValid",
                                      false, 3);
  EXPECT_EQ(VersionComparisonResult::kInvalidFormat,
            ExperimentConfigManager::CompareVersions("9.lts.0", "9.0"));
  histogram_tester_.ExpectBucketCount("Cobalt.Finch.VersionComparisonIsValid",
                                      false, 4);
}

TEST_F(ExperimentConfigManagerTest,
       GetExperimentConfigTypeUsesServerConfiguredThresholds) {
  base::Value::Dict finch_params;
  finch_params.Set(kCrashStreakSafeConfigThreshold, 5);
  finch_params.Set(kCrashStreakEmptyConfigThreshold, 10);
  pref_service_->SetDict(kFinchParameters, std::move(finch_params));

  // Crash streak is 4, which is less than the server-configured safe threshold
  // of 5. Expect kRegularConfig.
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    4);
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kRegularConfig);

  // Crash streak is 5, which is equal to the server-configured safe threshold.
  // Expect kSafeConfig.
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    5);
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kSafeConfig);

  // Crash streak is 9, which is between the safe and empty thresholds.
  // Expect kSafeConfig.
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    9);
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kSafeConfig);

  // Crash streak is 10, which is equal to the server-configured empty
  // threshold. Expect kEmptyConfig.
  metrics_pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                                    10);
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kEmptyConfig);
}

}  // namespace cobalt
