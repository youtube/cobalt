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

#include "starboard/system.h"

#include <string>

#include "base/android/jni_android.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_state.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/once.h"
#include "starboard/common/string.h"

namespace starboard::android::shared {
namespace {

// A singleton class to hold a locale string
class LocaleInfo {
 public:
  // The Starboard locale id
  std::string locale_id;

  LocaleInfo() {
    JNIEnv* env = base::android::AttachCurrentThread();

    ScopedLocalJavaRef<jstring> result(JniCallObjectMethodOrAbort(
        env, JNIState::GetStarboardBridge(), "systemGetLocaleId",
        "()Ljava/lang/String;"));
    locale_id = JniGetStringStandardUTFOrAbort(env, result.Get());
  }
};

SB_ONCE_INITIALIZE_FUNCTION(LocaleInfo, GetLocale)
}  // namespace
}  // namespace starboard::android::shared

const char* SbSystemGetLocaleId() {
  return starboard::android::shared::GetLocale()->locale_id.c_str();
}
