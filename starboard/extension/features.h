// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_EXTENSION_FEATURES_H_
#define STARBOARD_EXTENSION_FEATURES_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionFeaturesName "dev.starboard.extension.Features"

// Enum Class to help determine which value to access
// inside the |SbFeatureParamValue| union.
typedef enum SbFeatureParamType {
  SbFeatureParamTypeBool,
  SbFeatureParamTypeInt,
  SbFeatureParamTypeDouble,
  SbFeatureParamTypeString,
  SbFeatureParamTypeTime,
} SbFeatureParamType;

typedef struct SbFeature {
  const char* name;
  bool is_enabled;
} SbFeature;

typedef struct SbFeatureParam {
  const char* feature_name;
  const char* param_name;
  SbFeatureParamType type;
  union SbFeatureParamValue {
    bool bool_value;
    int int_value;
    double double_value;
    const char* string_value;
    // |time_value| holds a base::TimeDelta value converted into an int64_t
    // value in microseconds. The conversion is done through the base function
    // GetFieldTrialParamByFeatureAsTimeDelta().inMicroseconds();
    int64_t time_value;
  } value;
} SbFeatureParam;

typedef struct StarboardExtensionFeaturesApi {
  // Name should be the string |kStarboardExtensionFeaturesName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The function to initialize starboard features and params.
  // This will take in the features and params that are pushed
  // from the Chrobalt side to help populate the FeatureList in Starboard.
  // Use one function call for both features and params to maximize the
  // performance.
  void (*InitializeStarboardFeatures)(const SbFeature* features,
                                      size_t number_of_features,
                                      const SbFeatureParam* params,
                                      size_t number_of_params);
} StarboardExtensionFeaturesApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_FEATURES_H_
