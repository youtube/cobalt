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

#ifndef STARBOARD_SHARED_STARBOARD_FEATURES_H_
#define STARBOARD_SHARED_STARBOARD_FEATURES_H_

// #include "starboard/shared/starboard/feature_list.h"

#define STARBOARD_FEATURE(feature, name, default_state) \
  SB_DECLARE_FEATURE(feature);

#define STARBOARD_FEATURE_PARAM(T, param_object_name, feature, name, \
                                default_value)                       \
  SB_DECLARE_FEATURE_PARAM(T, param_object_name);

#define FEATURE_LIST_START namespace starboard::features {
#define FEATURE_LIST_END }

#define FEATURE_PARAM_LIST_START namespace starboard::features {
#define FEATURE_PARAM_LIST_END }

#include "starboard/common/feature_config.h"

#undef STARBOARD_FEATURE
#undef STARBOARD_FEATURE_PARAM
#undef FEATURE_LIST_START
#undef FEATURE_LIST_END
#undef FEATURE_PARAM_LIST_START
#undef FEATURE_PARAM_LIST_END

#endif  // STARBOARD_SHARED_STARBOARD_FEATURES_H_
