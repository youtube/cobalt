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
//
// ========================================================================
//
// Definition file to define Starboard features and parameters on the
// Cobalt level. This file uses starboard/extension/feature_config.h
// to know what features and parameters it will be defining. These features
// and parameters are usable on the Cobalt level, and are treated as if they
// were base::Features and base::FeatureParams.

#include "cobalt/common/features/features.h"

#define STARBOARD_FEATURE(feature, name, default_state)                 \
  BASE_FEATURE(feature, name,                                           \
               default_state == true ? base::FEATURE_ENABLED_BY_DEFAULT \
                                     : base::FEATURE_DISABLED_BY_DEFAULT);

#define STARBOARD_FEATURE_PARAM(T, param_object_name, feature_object_name,   \
                                param_name, default_value)                   \
  BASE_FEATURE_PARAM(T, param_object_name, &feature_object_name, param_name, \
                     default_value);

#define FEATURE_LIST_START namespace cobalt::features {
#define FEATURE_LIST_END }

#define FEATURE_PARAM_LIST_START namespace cobalt::features {
#define FEATURE_PARAM_LIST_END }

#define STARBOARD_FEATURE_PARAM_TIME_TYPE base::TimeDelta

#include "starboard/extension/feature_config.h"

#undef STARBOARD_FEATURE
#undef STARBOARD_FEATURE_PARAM
#undef FEATURE_LIST_START
#undef FEATURE_LIST_END
#undef FEATURE_PARAM_LIST_START
#undef FEATURE_PARAM_LIST_END
#undef STARBOARD_FEATURE_PARAM_TIME_TYPE
