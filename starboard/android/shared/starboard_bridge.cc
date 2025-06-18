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
#include "base/strings/string_number_conversions.h"
#include "cobalt/browser/client_hint_headers/cobalt_header_value_provider.h"
#include "cobalt/browser/h5vcc_runtime/deep_link_manager.h"
#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/file_internal.h"
#include "starboard/android/shared/log_internal.h"
#include "starboard/common/command_line.h"
#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/starboard/log_mutex.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/StarboardBridge_jni.h"

namespace starboard::android::shared {

// TODO: (cobalt b/372559388) Update namespace to jni_zero.
using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::GetClass;

// Global pointer to hold the single instance of ApplicationAndroid.
ApplicationAndroid* g_native_app_instance = nullptr;
static pthread_mutex_t g_native_app_init_mutex PTHREAD_MUTEX_INITIALIZER;

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

extern "C" SB_EXPORT_PLATFORM jlong JNI_StarboardBridge_StartNativeStarboard(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_asset_manager,
    const JavaParamRef<jstring>& j_files_dir,
    const JavaParamRef<jstring>& j_cache_dir,
    const JavaParamRef<jstring>& j_native_library_dir) {
#if SB_IS(EVERGREEN_COMPATIBLE)
  StarboardThreadLaunch();
#else
  pthread_mutex_lock(&g_native_app_init_mutex);
  if (g_native_app_instance == nullptr) {
    auto command_line = std::make_unique<CommandLine>(GetArgs());
    LogInit(*command_line);
    ScopedJavaGlobalRef<jobject> asset_manager(env, j_asset_manager.obj());
    g_native_app_instance = new ApplicationAndroid(
        std::move(command_line), std::move(asset_manager),
        ConvertJavaStringToUTF8(env, j_files_dir),
        ConvertJavaStringToUTF8(env, j_cache_dir),
        ConvertJavaStringToUTF8(env, j_native_library_dir));
    // Ensure application init happens here
    ApplicationAndroid::Get();
  }
  pthread_mutex_unlock(&g_native_app_init_mutex);
  return reinterpret_cast<jlong>(g_native_app_instance);
#endif  // SB_IS(EVERGREEN_COMPATIBLE)
}

extern "C" SB_EXPORT_PLATFORM void JNI_StarboardBridge_HandleDeepLink(
    JNIEnv* env,
    const JavaParamRef<jstring>& jurl,
    jboolean applicationStarted) {
  const std::string& url = ConvertJavaStringToUTF8(env, jurl);
  LOG(INFO) << "StarboardBridge handling DeepLink: " << url;

  auto* manager = cobalt::browser::DeepLinkManager::GetInstance();
  if (applicationStarted) {
    // Warm start deeplink
    manager->OnDeepLink(url);
  } else {
    // Cold start deeplink
    manager->set_deep_link(url);
  }
}

extern "C" SB_EXPORT_PLATFORM void JNI_StarboardBridge_SetAndroidOSExperience(
    JNIEnv* env,
    jboolean isAmatiDevice) {
  std::string value = isAmatiDevice ? "Amati" : "Watson";
  auto header_value_provider =
      cobalt::browser::CobaltHeaderValueProvider::GetInstance();
  header_value_provider->SetHeaderValue("Sec-CH-UA-Co-Android-OS-Experience",
                                        value);
}

extern "C" SB_EXPORT_PLATFORM void
JNI_StarboardBridge_SetAndroidPlayServicesVersion(JNIEnv* env, jlong version) {
  auto header_value_provider =
      cobalt::browser::CobaltHeaderValueProvider::GetInstance();
  header_value_provider->SetHeaderValue(
      "Sec-CH-UA-Co-Android-Play-Services-Version",
      base::NumberToString(version));
}

extern "C" SB_EXPORT_PLATFORM void
JNI_StarboardBridge_SetAndroidBuildFingerprint(
    JNIEnv* env,
    const JavaParamRef<jstring>& fingerprint) {
  auto header_value_provider =
      cobalt::browser::CobaltHeaderValueProvider::GetInstance();
  header_value_provider->SetHeaderValue(
      "Sec-CH-UA-Co-Android-Build-Fingerprint",
      ConvertJavaStringToUTF8(env, fingerprint));
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

long StarboardBridge::GetAppStartDuration(JNIEnv* env) {
  SB_DCHECK(env);
  return Java_StarboardBridge_getAppStartDuration(env, j_starboard_bridge_);
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
  return limit_ad_tracking_java == JNI_TRUE;
}

SB_EXPORT_ANDROID void StarboardBridge::CloseApp(JNIEnv* env) {
  SB_DCHECK(env);
  return Java_StarboardBridge_closeApp(env, j_starboard_bridge_);
}

std::string StarboardBridge::GetTimeZoneId(JNIEnv* env) {
  SB_DCHECK(env);
  ScopedJavaLocalRef<jstring> timezone_id_java =
      Java_StarboardBridge_getTimeZoneId(env, j_starboard_bridge_);
  return ConvertJavaStringToUTF8(env, timezone_id_java);
}

ScopedJavaLocalRef<jobject> StarboardBridge::GetDisplayDpi(JNIEnv* env) {
  SB_DCHECK(env);
  return Java_StarboardBridge_getDisplayDpi(env, j_starboard_bridge_);
}

ScopedJavaLocalRef<jobject> StarboardBridge::GetDeviceResolution(JNIEnv* env) {
  SB_DCHECK(env);
  return Java_StarboardBridge_getDisplayDpi(env, j_starboard_bridge_);
}

bool StarboardBridge::IsNetworkConnected(JNIEnv* env) {
  SB_DCHECK(env);
  return Java_StarboardBridge_isNetworkConnected(env, j_starboard_bridge_);
}

void StarboardBridge::ReportFullyDrawn(JNIEnv* env) {
  SB_DCHECK(env);
  return Java_StarboardBridge_reportFullyDrawn(env, j_starboard_bridge_);
}

ScopedJavaLocalRef<jobject> StarboardBridge::GetAudioOutputManager(
    JNIEnv* env) {
  SB_DCHECK(env);
  return Java_StarboardBridge_getAudioOutputManager(env, j_starboard_bridge_);
}

bool StarboardBridge::IsMicrophoneDisconnected(JNIEnv* env) {
  SB_DCHECK(env);
  return Java_StarboardBridge_isMicrophoneDisconnected(env,
                                                       j_starboard_bridge_);
}

bool StarboardBridge::IsMicrophoneMute(JNIEnv* env) {
  SB_DCHECK(env);
  return Java_StarboardBridge_isMicrophoneMute(env, j_starboard_bridge_);
}

ScopedJavaLocalRef<jobject> StarboardBridge::GetAudioPermissionRequester(
    JNIEnv* env) {
  SB_DCHECK(env);
  return Java_StarboardBridge_getAudioPermissionRequester(env,
                                                          j_starboard_bridge_);
}

void StarboardBridge::ResetVideoSurface(JNIEnv* env) {
  SB_DCHECK(env);
  return Java_StarboardBridge_resetVideoSurface(env, j_starboard_bridge_);
}

void StarboardBridge::SetVideoSurfaceBounds(JNIEnv* env,
                                            int x,
                                            int y,
                                            int width,
                                            int height) {
  SB_DCHECK(env);
  return Java_StarboardBridge_setVideoSurfaceBounds(env, j_starboard_bridge_, x,
                                                    y, width, height);
}

}  // namespace starboard::android::shared
