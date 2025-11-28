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

#ifndef COBALT_COMMON_FEATURES_COBALT_FEATURES_H_
#define COBALT_COMMON_FEATURES_COBALT_FEATURES_H_

#include "base/feature_list.h"

// Features defined here will be used in Cobalt's Java code.
namespace cobalt::features {

// |BUFFER_FLAG_DECODE_ONLY| is only used in tunnel playbacks
// to explicitly skip video frames before the seek time so that they won't
// be rendered. Set the following variable to true to use
// |BUFFER_FLAG_DECODE_ONLY| for non-tunneled playbacks as well.
BASE_DECLARE_FEATURE(kNonTunneledDecodeOnly);

}  // namespace cobalt::features

#endif  // COBALT_COMMON_FEATURES_COBALT_FEATURES_H_
