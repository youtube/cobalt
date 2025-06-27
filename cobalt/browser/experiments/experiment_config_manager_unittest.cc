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

#include "base/test/task_environment.h"
#include "base/values.h"
#include "cobalt/browser/constants/cobalt_experiment_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

class ExperimentConfigManagerTest : public testing::Test {
 public:
  ExperimentConfigManagerTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::DEFAULT) {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_->registry()->RegisterIntegerPref(
        variations::prefs::kVariationsCrashStreak, 0);
    pref_service_->registry()->RegisterDictionaryPref(
        kExperimentConfigFeatures);
    pref_service_->registry()->RegisterDictionaryPref(
        kExperimentConfigFeatureParams);
    pref_service_->registry()->RegisterListPref(kExperimentConfigExpIds);
    pref_service_->registry()->RegisterDictionaryPref(kSafeConfigFeatures);
    pref_service_->registry()->RegisterDictionaryPref(kSafeConfigFeatureParams);
    pref_service_->registry()->RegisterListPref(kSafeConfigExpIds);
    experiment_config_manager_ =
        std::make_unique<ExperimentConfigManager>(pref_service_.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<ExperimentConfigManager> experiment_config_manager_;
};

TEST_F(ExperimentConfigManagerTest, GetExperimentConfigTypeReturnsRegular) {
  pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak, 0);
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kRegularConfig);
}

TEST_F(ExperimentConfigManagerTest, GetExperimentConfigTypeReturnsSafe) {
  pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                            kCrashStreakSafeConfigThreshold);
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kSafeConfig);
}

TEST_F(ExperimentConfigManagerTest, GetExperimentConfigTypeReturnsEmpty) {
  pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                            kCrashStreakEmptyConfigThreshold);
  EXPECT_EQ(experiment_config_manager_->GetExperimentConfigType(),
            ExperimentConfigType::kEmptyConfig);
}

TEST_F(ExperimentConfigManagerTest, StoreSafeConfigIsNoOpForSafeConfig) {
  pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                            kCrashStreakSafeConfigThreshold);

  experiment_config_manager_->StoreSafeConfig();
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(experiment_config_manager_->has_called_store_safe_config());
  EXPECT_TRUE(pref_service_->GetDict(kSafeConfigFeatures).empty());
}

TEST_F(ExperimentConfigManagerTest, StoreSafeConfigIsNoOpForEmptyConfig) {
  pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak,
                            kCrashStreakEmptyConfigThreshold);

  experiment_config_manager_->StoreSafeConfig();
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(experiment_config_manager_->has_called_store_safe_config());
  EXPECT_TRUE(pref_service_->GetDict(kSafeConfigFeatures).empty());
}

TEST_F(ExperimentConfigManagerTest, StoreSafeConfigIsOnlyCalledOnce) {
  base::Value::Dict initial_features;
  initial_features.Set("feature1", true);
  pref_service_->SetDict(kExperimentConfigFeatures, initial_features.Clone());
  pref_service_->SetInteger(variations::prefs::kVariationsCrashStreak, 0);

  // First call should store the initial config.
  experiment_config_manager_->StoreSafeConfig();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(experiment_config_manager_->has_called_store_safe_config());
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

}  // namespace cobalt
