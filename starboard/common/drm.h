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

#ifndef STARBOARD_COMMON_DRM_H_
#define STARBOARD_COMMON_DRM_H_

#include <ostream>

#include "starboard/drm.h"

namespace starboard {

const char* GetSbDrmSessionRequestTypeName(
    SbDrmSessionRequestType request_type);

const char* GetSbDrmStatusName(SbDrmStatus status);

}  // namespace starboard

std::ostream& operator<<(std::ostream& os, SbDrmSessionRequestType type);

std::ostream& operator<<(std::ostream& os, SbDrmStatus status);

#endif  // STARBOARD_COMMON_DRM_H_
