// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_COMMON_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_COMMON_H_

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/string.h"

namespace starboard {
namespace android {
namespace shared {

const int64_t kSecondInMicroseconds = 1000 * 1000;
const char kMimeTypeAac[] = "audio/mp4a-latm";

inline SbMediaTime ConvertMicrosecondsToSbMediaTime(
    int64_t time_in_microseconds) {
  return time_in_microseconds * kSbMediaTimeSecond / kSecondInMicroseconds;
}

inline int64_t ConvertSbMediaTimeToMicroseconds(SbMediaTime media_time) {
  return media_time * kSecondInMicroseconds / kSbMediaTimeSecond;
}

inline bool IsWidevine(const char* key_system) {
  return SbStringCompareAll(key_system, "com.widevine") == 0 ||
         SbStringCompareAll(key_system, "com.widevine.alpha") == 0;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_COMMON_H_
