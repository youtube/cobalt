// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/application_android.h"

int64_t SbMediaGetBufferGarbageCollectionDurationThreshold() {
  const int64_t overlayed_threshold =
      starboard::android::shared::ApplicationAndroid::Get()
          ->GetOverlayedIntValue(
              "buffer_garbage_collection_duration_threshold");
  if (overlayed_threshold != 0) {
    return overlayed_threshold;
  }
  return 170'000'000;  // 170 seconds
}
