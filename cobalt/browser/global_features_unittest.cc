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

#include "cobalt/browser/global_features.h"

#include "base/base_paths_posix.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "cobalt/browser/constants/cobalt_experiment_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

class GlobalFeaturesTest : public testing::Test {
 public:
  GlobalFeaturesTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::DEFAULT) {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::ScopedPathOverride cache_override(base::DIR_CACHE,
                                            temp_dir_.GetPath(), true, true);
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

// TODO(b/419612226): test case crashing on CI and not reproducible locally.
TEST_F(GlobalFeaturesTest, DISABLED_CreatesPrefServiceOnInitialization) {
  GlobalFeatures* instance = GlobalFeatures::GetInstance();
  EXPECT_NE(nullptr, instance->experiment_config());
  EXPECT_NE(nullptr, instance->metrics_local_state());
  EXPECT_NE(nullptr, instance->metrics_services_manager());
  EXPECT_NE(nullptr, instance->metrics_services_manager_client());
  EXPECT_NE(nullptr, instance->metrics_service());
}

TEST_F(GlobalFeaturesTest, GetInstanceReturnsSameInstance) {
  GlobalFeatures* instance1 = GlobalFeatures::GetInstance();
  GlobalFeatures* instance2 = GlobalFeatures::GetInstance();
  EXPECT_EQ(instance1, instance2);
}

TEST_F(GlobalFeaturesTest, GetMetricsServicesManagerReturnsSameInstance) {
  GlobalFeatures* instance = GlobalFeatures::GetInstance();
  metrics_services_manager::MetricsServicesManager* manager1 =
      instance->metrics_services_manager();
  metrics_services_manager::MetricsServicesManager* manager2 =
      instance->metrics_services_manager();
  EXPECT_EQ(manager1, manager2);
  EXPECT_NE(nullptr, manager1);
}

TEST_F(GlobalFeaturesTest, RegisterPrefsRegistersExpectedPrefs) {
  TestingPrefServiceSimple pref_service;
  GlobalFeatures::RegisterPrefs(pref_service.registry());

  EXPECT_TRUE(pref_service.FindPreference(kExperimentConfig));
  EXPECT_TRUE(pref_service.FindPreference(kExperimentConfigFeatures));
  EXPECT_TRUE(pref_service.FindPreference(kExperimentConfigFeatureParams));
  EXPECT_TRUE(pref_service.FindPreference(kExperimentConfigExpIds));
}

TEST_F(GlobalFeaturesTest, ActiveExperimentIdsUnchangedAfterInitialization) {
  auto active_experiment_ids =
      GlobalFeatures::GetInstance()->active_experiment_ids();
  EXPECT_TRUE(active_experiment_ids.empty());

  base::FilePath config_file =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Experiment Config"));
  base::WriteFile(config_file,
                  R"({"features": {"feature_a": true},
                     "feature_params": {"param1": "value1"},
                     "exp_ids": [1234]})");

  active_experiment_ids =
      GlobalFeatures::GetInstance()->active_experiment_ids();
  EXPECT_TRUE(active_experiment_ids.empty());
}

}  // namespace cobalt
