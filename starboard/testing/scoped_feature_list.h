// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_TESTING_SCOPED_FEATURE_LIST_H_
#define STARBOARD_TESTING_SCOPED_FEATURE_LIST_H_

#include <optional>
#include <string>
#include <vector>

#include "starboard/extension/features.h"

namespace starboard::features {

// Helper class to override features in tests.
// This follows the pattern of base::test::ScopedFeatureList in Chromium.
//
// Expected lifetime and ownership:
// This class is designed to be stack-allocated (RAII). Its lifetime should
// be scoped to the block or test where the overrides are needed. Upon
// destruction, it automatically restores the original feature states.
//
// Threading model:
// This class is NOT thread-safe. It modifies global state in FeatureList,
// so it should only be used in single-threaded test contexts or when access
// to FeatureList is otherwise synchronized. Multiple ScopedFeatureList
// instances should not be active concurrently if they modify the same features.
class ScopedFeatureList {
 public:
  ScopedFeatureList() = default;
  ~ScopedFeatureList();

  // Disallow copy and assign.
  ScopedFeatureList(const ScopedFeatureList&) = delete;
  ScopedFeatureList& operator=(const ScopedFeatureList&) = delete;

  // Overrides the given feature to be enabled.
  void InitAndEnableFeature(const SbFeature& feature);
  void InitAndEnableFeature(const std::string& feature_name);

  // Overrides the given feature to be disabled.
  void InitAndDisableFeature(const SbFeature& feature);
  void InitAndDisableFeature(const std::string& feature_name);

  // Resets all overrides made by this instance.
  void Reset();

 private:
  struct OriginalOverride {
    std::string feature_name;
    std::optional<bool> value;
  };
  std::vector<OriginalOverride> overridden_features_;

  void SaveOverride(const std::string& feature_name);
};

}  // namespace starboard::features

#endif  // STARBOARD_TESTING_SCOPED_FEATURE_LIST_H_
