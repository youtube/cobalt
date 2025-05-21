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

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/mutex.h"
#include "starboard/common/once.h"
#include "starboard/common/string.h"

using starboard::android::shared::JniEnvExt;
using starboard::android::shared::ScopedLocalJavaRef;

namespace {

static starboard::Mutex g_locale_mutex;
static std::string locale_id;

}  // namespace

const char* SbSystemGetLocaleId() {
  starboard::ScopedLock lock(g_locale_mutex);
  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jstring> result(env->CallStarboardObjectMethodOrAbort(
      "systemGetLocaleId", "()Ljava/lang/String;"));
  locale_id = env->GetStringStandardUTFOrAbort(result.Get());
  return locale_id.c_str();
}
