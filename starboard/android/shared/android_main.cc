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

#include "build/build_config.h"
#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/export.h"

namespace starboard::android::shared {

namespace {

extern "C" SB_EXPORT_PLATFORM jboolean
Java_dev_cobalt_coat_StarboardBridge_isReleaseBuild() {
#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  return true;
#else
  return false;
#endif
}

// TODO(cobalt, b/372559388): consolidate this function when fully deprecate
// JniEnvExt.
extern "C" SB_EXPORT_PLATFORM void Java_dev_cobalt_coat_StarboardBridge_initJNI(
    JniEnvExt* env,
    jobject starboard_bridge) {
  JniEnvExt::Initialize(env, starboard_bridge);

  // Initialize the singleton instance of StarboardBridge
  JNIEnv* jni_env = base::android::AttachCurrentThread();
  StarboardBridge::GetInstance()->Initialize(jni_env, starboard_bridge);
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_coat_StarboardBridge_closeNativeStarboard(JniEnvExt* env,
                                                          jlong nativeApp) {
  auto* app = reinterpret_cast<ApplicationAndroid*>(nativeApp);
  delete app;
}

}  // namespace

}  // namespace starboard::android::shared
