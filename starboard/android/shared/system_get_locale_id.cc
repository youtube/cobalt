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
#include "starboard/system.h"
// clang-format on

#include <string>

#include "base/android/jni_string.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/once.h"
#include "starboard/common/string.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {

namespace {

// A singleton class to hold a locale string
class LocaleInfo {
 public:
  // The Starboard locale id
  std::string locale_id;

  LocaleInfo() {
    JNIEnv* env = jni_zero::AttachCurrentThread();

    jni_zero::ScopedJavaLocalRef<jstring> result =
        StarboardBridge::GetInstance()->GetSystemLocaleId(env);
    locale_id = base::android::ConvertJavaStringToUTF8(env, result.obj());
  }
};

SB_ONCE_INITIALIZE_FUNCTION(LocaleInfo, GetLocale)
}  // namespace
}  // namespace starboard

const char* SbSystemGetLocaleId() {
  return starboard::GetLocale()->locale_id.c_str();
}
