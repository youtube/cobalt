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

#include "cobalt/common/features/features.h"

namespace cobalt {
namespace features {

#define FEATURE_LIST_START \
  constexpr const base::Feature* kStarboardFeatures[] = {
#define FEATURE_LIST_END \
  }                      \
  ;

#define FEATURE_PARAM_LIST_START \
  constexpr const FeatureParamWrapper kStarboardParams[] = {
#define FEATURE_PARAM_LIST_END \
  }                            \
  ;

#define STARBOARD_FEATURE(feature, name, default_state) &feature,

#define STARBOARD_FEATURE_PARAM(T, param_object_name, feature, name, \
                                default_value)                       \
  SbFeatureParamFromBaseFeatureParam(param_object_name),

#include "starboard/common/feature_config.h"

#undef STARBOARD_FEATURE
#undef STARBOARD_FEATURE_PARAM
#undef FEATURE_LIST_START
#undef FEATURE_LIST_END
#undef FEATURE_PARAM_LIST_START
#undef FEATURE_PARAM_LIST_END

#define ARRAY_SIZE(arr) ((sizeof(array) / sizeof(array[0])))
// This function should be called right after Finch experiment states
// are loaded.
void InitializeStarboardFeatures() {
  const StarboardExtensionMediaFeaturesApi* extension_api =
      static_cast<const StarboardExtensionMediaFeaturesApi*>(
          SbSystemGetExtension(kStarboardExtensionMediaFeaturesName));
  if (!extension_api) {
    return;
  }
  DCHECK_EQ(extension_api->name, kStarboardExtensionMediaFeaturesName);
  DCHECK_EQ(extension_api->version, 1u);
  DCHECK(extension_api->InitializeStarboardFeatures);

  size_t number_of_features = ARRAY_SIZE(kStarboardFeatures);
  size_t number_of_params = ARRAY_SIZE(kStarboardParams);
  std::vector<SbMediaFeature> features();
  std::vector<SbMediaFeatureParam> params();
  // base::FeatureList::CheckFeatureIdentity checks the identity of
  // features and only allows one instance of a Feature struct for a
  // given name. So, we need to hold and reuse the Feature instances
  // created dynamically here.
  //... read... features... from... kStarboardFeatures...and...
  // kStarboardParams...... retrieve... features... by... base::
  //    FeatureList::
  //        GetStateIfOverridden()...... retrieve... params... by... base::
  //           GetFieldTrialParamValueByFeature()...

  // only send down overridden params, read through params and then
  // fill an array

  extension_api->InitializeStarboardFeatures(features.data(), features.size(),
                                             params.data(), params.size());
}
}  // namespace features
}  // namespace cobalt
