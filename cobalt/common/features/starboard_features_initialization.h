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

#ifndef COBALT_COMMON_FEATURES_STARBOARD_FEATURES_INITIALIZATION_H_
#define COBALT_COMMON_FEATURES_STARBOARD_FEATURES_INITIALIZATION_H_

namespace cobalt::features {

// InitializeStarboardFeatures() will access the Starboard Features
// extension to send the updated features and params defined in
// starboard/extension/feature_config.h and send them down to the
// starboard level.

void InitializeStarboardFeatures();

}  // namespace cobalt::features

#endif  // COBALT_COMMON_FEATURES_STARBOARD_FEATURES_INITIALIZATION_H_
