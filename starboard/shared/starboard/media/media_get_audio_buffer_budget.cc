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
#define LEGACY_NON_VIDEO_BUDGET 5 * 1024 * 1024

int SbMediaGetAudioBufferBudget() {
#if defined(COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET) && \
    COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET != LEGACY_NON_VIDEO_BUDGET
  SB_DLOG(WARNING) << "COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET will be deprecated "
                      "in a future Starboard version.";
  // Use define forwarded from GYP variable.
  return COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET;
#else   // defined(COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET) &&
  // COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET != LEGACY_NON_VIDEO_BUDGET
  return 5 * 1024 * 1024;
#endif  // defined(COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET) &&
        // COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET != LEGACY_NON_VIDEO_BUDGET
}
#endif  // SB_API_VERSION >= 10
