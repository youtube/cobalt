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

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/file_internal.h"
#include "starboard/android/shared/log_internal.h"
#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/starboard/command_line.h"
#include "starboard/shared/starboard/log_mutex.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/StarboardBridge_jni.h"

namespace starboard {
namespace android {
namespace shared {

namespace {
#if SB_IS(EVERGREEN_COMPATIBLE)
void StarboardThreadLaunch() {
  // Start the Starboard thread the first time an Activity is created.
  if (g_starboard_thread == 0) {
    Semaphore semaphore;

    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);

    pthread_create(&g_starboard_thread, &attributes, &ThreadEntryPoint,
                   &semaphore);

    pthread_attr_destroy(&attributes);

    // Wait for the ApplicationAndroid to be created.
    semaphore.Take();
  }

  // Ensure application init happens here
  ApplicationAndroid::Get();
}
#endif  // SB_IS(EVERGREEN_COMPATIBLE)
}  // namespace

std::vector<std::string> GetArgs() {
  std::vector<std::string> args;
  // Fake program name as args[0]
  args.push_back("android_main");

  JNIEnv* env = base::android::AttachCurrentThread();
  StarboardBridge::GetInstance()->AppendArgs(env, &args);

  return args;
}

extern "C" SB_EXPORT_PLATFORM void JNI_StarboardBridge_OnStop(JNIEnv* env) {
  ::starboard::shared::starboard::audio_sink::SbAudioSinkImpl::TearDown();
  SbFileAndroidTeardown();
}

extern "C" SB_EXPORT_PLATFORM jlong
JNI_StarboardBridge_CurrentMonotonicTime(JNIEnv* env) {
  return CurrentMonotonicTime();
}

extern "C" SB_EXPORT_PLATFORM jlong
JNI_StarboardBridge_StartNativeStarboard(JNIEnv* env) {
#if SB_IS(EVERGREEN_COMPATIBLE)
  StarboardThreadLaunch();
#else
  auto command_line = std::make_unique<CommandLine>(GetArgs());
  LogInit(*command_line);
  auto* nativeApp = new ApplicationAndroid(std::move(command_line));
  // Ensure application init happens here
  ApplicationAndroid::Get();
  return reinterpret_cast<jlong>(nativeApp);
#endif  // SB_IS(EVERGREEN_COMPATIBLE)
}

// StarboardBridge::GetInstance() should not be inlined in the
// header. This makes sure that when source files from multiple targets include
// this header they don't end up with different copies of the inlined code
// creating multiple copies of the singleton.
// static
StarboardBridge* StarboardBridge::GetInstance() {
  return base::Singleton<StarboardBridge>::get();
}

void StarboardBridge::Initialize(JNIEnv* env, jobject obj) {
  j_starboard_bridge_.Reset(env, obj);
}

long StarboardBridge::GetAppStartTimestamp(JNIEnv* env) {
  SB_DCHECK(env);
  return Java_StarboardBridge_getAppStartTimestamp(env, j_starboard_bridge_);
}

void StarboardBridge::ApplicationStarted(JNIEnv* env) {
  SB_DCHECK(env);
  Java_StarboardBridge_applicationStarted(env, j_starboard_bridge_);
}

void StarboardBridge::ApplicationStopping(JNIEnv* env) {
  SB_DCHECK(env);
  Java_StarboardBridge_applicationStopping(env, j_starboard_bridge_);
}

void StarboardBridge::AfterStopped(JNIEnv* env) {
  SB_DCHECK(env);
  Java_StarboardBridge_afterStopped(env, j_starboard_bridge_);
}

void StarboardBridge::AppendArgs(JNIEnv* env,
                                 std::vector<std::string>* args_vector) {
  SB_DCHECK(env);
  base::android::ScopedJavaLocalRef<jobjectArray> args_java =
      Java_StarboardBridge_getArgs(env, j_starboard_bridge_);
  base::android::AppendJavaStringArrayToStringVector(env, args_java,
                                                     args_vector);
}

std::string StarboardBridge::GetStartDeepLink(JNIEnv* env) {
  SB_DCHECK(env);
  base::android::ScopedJavaLocalRef<jstring> start_deep_link_java =
      Java_StarboardBridge_getStartDeepLink(env, j_starboard_bridge_);
  std::string start_deep_link =
      base::android::ConvertJavaStringToUTF8(env, start_deep_link_java);
  return start_deep_link;
}

base::android::ScopedJavaLocalRef<jintArray>
StarboardBridge::GetSupportedHdrTypes(JNIEnv* env) {
  SB_DCHECK(env);
  return Java_StarboardBridge_getSupportedHdrTypes(env, j_starboard_bridge_);
}
}  // namespace shared
}  // namespace android
}  // namespace starboard
