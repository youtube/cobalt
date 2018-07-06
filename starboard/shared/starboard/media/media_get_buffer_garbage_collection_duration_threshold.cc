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

#include "starboard/log.h"

#if SB_API_VERSION >= SB_MEDIA_BUFFER_SETTINGS_QUERIES_API_VERSION
SbTime SbMediaGetBufferGarbageCollectionDurationThreshold() {
#if defined( \
    COBALT_MEDIA_SOURCE_GARBAGE_COLLECTION_DURATION_THRESHOLD_IN_SECONDS)
  SB_DLOG(WARNING) << "COBALT_MEDIA_SOURCE_GARBAGE_COLLECTION_DURATION_"
                      "THRESHOLD will be deprecated in a future Starboard "
                      "version.";
  // Use define forwarded from GYP variable.
  return COBALT_MEDIA_SOURCE_GARBAGE_COLLECTION_DURATION_THRESHOLD_IN_SECONDS *
         kSbTimeSecond;
#else  // defined(COBALT_MEDIA_SOURCE_GARBAGE_COLLECTION_DURATION_THRESHOLD_IN_SECONDS)
  return 170 * kSbTimeSecond;
#endif  // defined(COBALT_MEDIA_SOURCE_GARBAGE_COLLECTION_DURATION_THRESHOLD_IN_SECONDS)
}
#endif  // SB_API_VERSION >= SB_MEDIA_BUFFER_SETTINGS_QUERIES_API_VERSION
