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

#include "starboard/shared/starboard/feature_list.h"

#include "starboard/common/log.h"
#include "starboard/common/once.h"

namespace starboard::features {

SB_ONCE_INITIALIZE_FUNCTION(FeatureList, FeatureList::GetInstance);

// static
void FeatureList::InitializeFeatureList(const SbFeature* features,
                                        size_t number_of_features,
                                        const SbFeatureParam* params,
                                        size_t number_of_params) {
  // Initialize the singleton instance via GetInstance();

  for (int i = 0; i < number_of_features; i++) {
    SB_LOG(INFO) << "John the name of the feature is: " << features[i].name;
  }
}
/*
template <>
int FeatureList::GetParam(const SbFeatureParamExt<int> &param) {
  SbFeatureParam* cached_param = SearchFromCachedFeatureParams();
  SB_CHECK(cached_param);

  return cached_param->int_value;
}
*/
}  // namespace starboard::features
