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
#include "cobalt/browser/h5vcc_runtime/deep_link_manager.h"
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

// TODO: (cobalt b/372559388) Update namespace to jni_zero.
using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::GetClass;

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

  JNIEnv* env = AttachCurrentThread();
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
  auto* native_app = new ApplicationAndroid(std::move(command_line));
  // Ensure application init happens here
  ApplicationAndroid::Get();
  return reinterpret_cast<jlong>(native_app);
#endif  // SB_IS(EVERGREEN_COMPATIBLE)
}

extern "C" SB_EXPORT_PLATFORM void JNI_StarboardBridge_HandleDeepLink(
    JNIEnv* env,
    const JavaParamRef<jstring>& jurl,
    jboolean applicationReady) {
  const std::string& url = base::android::ConvertJavaStringToUTF8(env, jurl);

  auto* manager = cobalt::browser::DeepLinkManager::GetInstance();
  if (applicationReady) {
    // TODO(cobalt, b/374147993): handle warm start deeplink
  } else {
    // Cold start deeplink
    manager->set_deep_link(url);
  }
}

// StarboardBridge::GetInstance() should not be inlined in the
// header. This makes sure that when source files from multiple targets include
// this header they don't end up with different copies of the inlined code
// creating multiple copies of the singleton.
// static
SB_EXPORT_ANDROID StarboardBridge* StarboardBridge::GetInstance() {
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
  ScopedJavaLocalRef<jobjectArray> args_java =
      Java_StarboardBridge_getArgs(env, j_starboard_bridge_);
  AppendJavaStringArrayToStringVector(env, args_java, args_vector);
}

ScopedJavaLocalRef<jintArray> StarboardBridge::GetSupportedHdrTypes(
    JNIEnv* env) {
  SB_DCHECK(env);
  return Java_StarboardBridge_getSupportedHdrTypes(env, j_starboard_bridge_);
}

void StarboardBridge::RaisePlatformError(JNIEnv* env,
                                         jint errorType,
                                         jlong data) {
  SB_DCHECK(env);
  Java_StarboardBridge_raisePlatformError(env, j_starboard_bridge_, errorType,
                                          data);
}

void StarboardBridge::RequestSuspend(JNIEnv* env) {
  SB_DCHECK(env);
  Java_StarboardBridge_requestSuspend(env, j_starboard_bridge_);
}

ScopedJavaLocalRef<jobject> StarboardBridge::GetApplicationContext(
    JNIEnv* env) {
  SB_DCHECK(env);
  return Java_StarboardBridge_getApplicationContext(env, j_starboard_bridge_);
}

ScopedJavaGlobalRef<jobject> StarboardBridge::GetAssetsFromContext(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject>& context) {
  SB_DCHECK(env);
  ScopedJavaLocalRef<jclass> context_class(
      GetClass(env, "android/content/Context"));
  jmethodID get_assets_method = env->GetMethodID(
      context_class.obj(), "getAssets", "()Landroid/content/res/AssetManager;");
  ScopedJavaLocalRef<jobject> asset_manager(
      env, env->CallObjectMethod(context.obj(), get_assets_method));
  ScopedJavaGlobalRef<jobject> global_asset_manager;
  global_asset_manager.Reset(asset_manager);
  return global_asset_manager;
}

std::string StarboardBridge::GetNativeLibraryDirFromContext(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject>& context) {
  SB_DCHECK(env);
  ScopedJavaLocalRef<jclass> context_class(
      GetClass(env, "android/content/Context"));
  jmethodID get_application_info_method =
      env->GetMethodID(context_class.obj(), "getApplicationInfo",
                       "()Landroid/content/pm/ApplicationInfo;");
  ScopedJavaLocalRef<jobject> application_info(
      env, env->CallObjectMethod(context.obj(), get_application_info_method));

  ScopedJavaLocalRef<jclass> application_info_class(
      env, env->GetObjectClass(application_info.obj()));
  jfieldID native_library_dir_field = env->GetFieldID(
      application_info_class.obj(), "nativeLibraryDir", "Ljava/lang/String;");
  ScopedJavaLocalRef<jstring> native_library_dir_java(
      env, static_cast<jstring>(env->GetObjectField(application_info.obj(),
                                                    native_library_dir_field)));
  std::string native_library_dir =
      ConvertJavaStringToUTF8(env, native_library_dir_java.obj());
  return native_library_dir.c_str();
}

std::string StarboardBridge::GetFilesAbsolutePath(JNIEnv* env) {
  SB_DCHECK(env);
  ScopedJavaLocalRef<jstring> file_path_java =
      Java_StarboardBridge_getFilesAbsolutePath(env, j_starboard_bridge_);
  std::string file_path = ConvertJavaStringToUTF8(env, file_path_java);
  return file_path;
}

std::string StarboardBridge::GetCacheAbsolutePath(JNIEnv* env) {
  SB_DCHECK(env);
  ScopedJavaLocalRef<jstring> file_path_java =
      Java_StarboardBridge_getCacheAbsolutePath(env, j_starboard_bridge_);
  std::string file_path = ConvertJavaStringToUTF8(env, file_path_java);
  return file_path;
}

ScopedJavaLocalRef<jobject> StarboardBridge::GetTextToSpeechHelper(
    JNIEnv* env) {
  SB_DCHECK(env);
  return Java_StarboardBridge_getTextToSpeechHelper(env, j_starboard_bridge_);
}

SB_EXPORT_ANDROID std::string StarboardBridge::GetAdvertisingId(JNIEnv* env) {
  SB_DCHECK(env);
  ScopedJavaLocalRef<jstring> advertising_id_java =
      Java_StarboardBridge_getAdvertisingId(env, j_starboard_bridge_);
  return ConvertJavaStringToUTF8(env, advertising_id_java);
}

SB_EXPORT_ANDROID bool StarboardBridge::GetLimitAdTracking(JNIEnv* env) {
  SB_DCHECK(env);
  jboolean limit_ad_tracking_java =
      Java_StarboardBridge_getLimitAdTracking(env, j_starboard_bridge_);
  return limit_ad_tracking_java;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
