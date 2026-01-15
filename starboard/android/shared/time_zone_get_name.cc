// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

// clang-format off
#include "starboard/time_zone.h"
// clang-format on

#include <time.h>

#include "starboard/android/shared/starboard_bridge.h"

// TODO: (cobalt b/372559388) Update namespace to jni_zero.
using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

const char* SbTimeZoneGetName() {
  static char s_time_zone_id[64];
  // Note tzset() is called in ApplicationAndroid::Initialize()
  JNIEnv* env = AttachCurrentThread();
  std::string time_zone_id =
      starboard::StarboardBridge::GetInstance()->GetTimeZoneId(env);

  time_zone_id.push_back('\0');
  strncpy(s_time_zone_id, time_zone_id.c_str(), sizeof(s_time_zone_id));
  s_time_zone_id[sizeof(s_time_zone_id) - 1] = 0;
  return s_time_zone_id;
}
