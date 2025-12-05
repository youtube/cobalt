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

#ifndef COBALT_BROWSER_EXPERIMENTS_EXPERIMENT_CONFIG_MANAGER_H_
#define COBALT_BROWSER_EXPERIMENTS_EXPERIMENT_CONFIG_MANAGER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_service.h"

namespace cobalt {

enum class ExperimentConfigType {
  kRegularConfig,
  kSafeConfig,
  kEmptyConfig,
};

// This class manages the content of the experiment config stored on disk.
class ExperimentConfigManager {
 public:
  explicit ExperimentConfigManager(PrefService* experiment_config,
                                   PrefService* metrics_local_state);
  // Returns the experiment config type based on the number of crashes and
  // whether the config has expired.
  ExperimentConfigType GetExperimentConfigType();

  // If regular config is used in the current run, save the active config as
  // safe config.
  // This should only be called before any modification to
  // variations::prefs::kVariationsCrashStreak.
  void StoreSafeConfig();

  // Public getter for testing.
  bool has_called_store_safe_config_for_testing() {
    return called_store_safe_config_;
  }

 private:
  bool called_store_safe_config_ = false;

  // PrefService for experiment config.
  const raw_ptr<PrefService> experiment_config_;
  // PrefService for metrics local state.
  const raw_ptr<PrefService> metrics_local_state_;

  FRIEND_TEST_ALL_PREFIXES(ExperimentConfigManagerTest,
                           StoreSafeConfigWithRegularConfig);
  FRIEND_TEST_ALL_PREFIXES(ExperimentConfigManagerTest,
                           StoreSafeConfigWithSafeConfig);
  FRIEND_TEST_ALL_PREFIXES(ExperimentConfigManagerTest,
                           StoreSafeConfigWithEmptyConfig);
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_EXPERIMENTS_EXPERIMENT_CONFIG_MANAGER_H_
