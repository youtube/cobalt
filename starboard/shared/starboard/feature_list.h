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

#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "starboard/extension/features.h"

namespace starboard {
namespace features {

class FeatureList {
 public:
  // ...
  static FeatureList* GetInstance();
  // ...
  static void InitializeFeatureList(const SbFeature* features,
                                    size_t number_of_features,
                                    const SbFeatureParam* params,
                                    size_t number_of_params);
  // ...
  // Add SB_DCHECK() to reject queries before initialized.
  static bool IsEnabled(const SbFeature& feature);
  // ...
  // Returns null if the param cannot be found.
  // Add SB_DCHECK() to reject queries before initialized.
  static std::optional<std::string> GetParam(const SbFeatureParam& param);

 private:
  FeatureList();
  ~FeatureList();

  // Mutex mutex_;
  std::vector<SbFeature> features_;
  std::vector<SbFeatureParam> params_;
};

template <typename T>
struct SbFeatureParamExt {  // TODO: Add inheritance from SbFeatureParam
  static_assert(std::is_same_v<bool, T> || std::is_same_v<int, T> ||
                    std::is_same_v<size_t, T> || std::is_same_v<double, T> ||
                    std::is_same_v<std::string, T> ||
                    std::is_same_v<int64_t, T>,
                "Unsupported Starboard FeatureParam<> type");
};

}  // namespace features
}  // namespace starboard
