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

#include "starboard/android/shared/starboard_bridge.h"

#include "starboard/android/shared/file_internal.h"
#include "starboard/common/time.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/StarboardBridge_jni.h"

namespace starboard {
namespace android {
namespace shared {

extern "C" SB_EXPORT_PLATFORM void JNI_StarboardBridge_OnStop(JNIEnv* env) {
  SbAudioSinkPrivate::TearDown();
  SbFileAndroidTeardown();
}

extern "C" SB_EXPORT_PLATFORM jlong
JNI_StarboardBridge_CurrentMonotonicTime(JNIEnv* env) {
  return CurrentMonotonicTime();
}

// static
StarboardBridge* StarboardBridge::GetInstance() {
  return base::Singleton<StarboardBridge>::get();
}

void StarboardBridge::Initialize(JNIEnv* env, jobject obj) {
  j_starboard_bridge_.Reset(env, obj);
}

long StarboardBridge::GetAppStartTimestamp() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  return Java_StarboardBridge_getAppStartTimestamp(env, j_starboard_bridge_);
}

void StarboardBridge::ApplicationStarted() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  return Java_StarboardBridge_applicationStarted(env, j_starboard_bridge_);
}

void StarboardBridge::ApplicationStopping() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  return Java_StarboardBridge_applicationStopping(env, j_starboard_bridge_);
}
}  // namespace shared
}  // namespace android
}  // namespace starboard
