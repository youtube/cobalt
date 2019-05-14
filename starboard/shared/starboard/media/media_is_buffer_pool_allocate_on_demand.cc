// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/media.h"

#include "starboard/common/log.h"

#if SB_API_VERSION >= 10
// This is the legacy default value of the GYP variable.
#define LEGACY_ALLOCATE_ON_DEMAND 1

bool SbMediaIsBufferPoolAllocateOnDemand() {
#if defined(COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND) && \
    COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND != LEGACY_ALLOCATE_ON_DEMAND
#pragma message(                                                           \
    "COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND will be deprecated in a " \
    "future Starboard version.")
  return static_cast<bool>(COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND);
#else   // defined(COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND) &&
  // COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND !=
  // LEGACY_ALLOCATE_ON_DEMAND
  return true;
#endif  // defined(COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND) &&
        // COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND !=
        // LEGACY_ALLOCATE_ON_DEMAND
}
#endif  // SB_API_VERSION >= 10
