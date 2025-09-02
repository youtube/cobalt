// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "base/android/jni_android.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_state.h"

using starboard::android::shared::JniCallVoidMethodOrAbort;
using starboard::android::shared::JNIState;

void SbSystemRequestStop(int error_level) {
  JNIEnv* env = base::android::AttachCurrentThread();
  JniCallVoidMethodOrAbort(env, JNIState::GetStarboardBridge(), "requestStop",
                           "(I)V", error_level);
}
