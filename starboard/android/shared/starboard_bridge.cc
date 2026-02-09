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
#include "build/build_config.h"
#include "cobalt/browser/client_hint_headers/cobalt_header_value_provider.h"
#include "cobalt/browser/h5vcc_runtime/deep_link_manager.h"
#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/file_internal.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/log_internal.h"
#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/starboard/command_line.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/StarboardBridge_jni.h"

namespace starboard::android::shared {

namespace {

// TODO: (cobalt b/372559388) Update namespace to jni_zero.
using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::GetClass;

// Client Hint Header name constants
constexpr char kAndroidOSExperienceHeader[] =
    "Sec-CH-UA-Co-Android-OS-Experience";
constexpr char kPlayServicesVersionHeader[] =
    "Sec-CH-UA-Co-Android-Play-Services-Version";
constexpr char kBuildFingerprintHeader[] =
    "Sec-CH-UA-Co-Android-Build-Fingerprint";
constexpr char kYoutubeCertScopeHeader[] =
    "Sec-CH-UA-Co-Youtube-Certification-Scope";

// Global pointer to hold the single instance of ApplicationAndroid.
ApplicationAndroid* g_native_app_instance = nullptr;
static pthread_mutex_t g_native_app_init_mutex PTHREAD_MUTEX_INITIALIZER;

std::vector<std::string> GetArgs() {
  std::vector<std::string> args;
  // Fake program name as args[0]
  args.push_back("android_main");

  JNIEnv* env = AttachCurrentThread();
  StarboardBridge::GetInstance()->AppendArgs(env, &args);

  return args;
}

}  // namespace

// TODO: b/372559388 - Consolidate this function when fully deprecate
// JniEnvExt.
jboolean JNI_StarboardBridge_InitJNI(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_starboard_bridge) {
  // This downcast is safe, since JniEnvExt adds only methods, not member
  // variables.
  // https://github.com/youtube/cobalt/blob/88c9c68/starboard/android/shared/jni_env_ext.cc#L90-L91
  auto env_ext = static_cast<JniEnvExt*>(env);
  SB_CHECK(env_ext);
  JniEnvExt::Initialize(env_ext, j_starboard_bridge.obj());

  // Initialize the singleton instance of StarboardBridge
  StarboardBridge::GetInstance()->Initialize(env, j_starboard_bridge.obj());

  StarboardBridge::GetInstance()->SetStartupMilestone(5);
  return true;
}

jlong JNI_StarboardBridge_CurrentMonotonicTime(JNIEnv* env) {
  return CurrentMonotonicTime();
}

jlong JNI_StarboardBridge_StartNativeStarboard(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_asset_manager,
    const JavaParamRef<jstring>& j_files_dir,
    const JavaParamRef<jstring>& j_cache_dir,
    const JavaParamRef<jstring>& j_native_library_dir) {
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
  }
  pthread_mutex_unlock(&g_native_app_init_mutex);
  return reinterpret_cast<jlong>(g_native_app_instance);
}

void JNI_StarboardBridge_CloseNativeStarboard(JNIEnv* env, jlong nativeApp) {
  auto* app = reinterpret_cast<ApplicationAndroid*>(nativeApp);
  delete app;
}

void JNI_StarboardBridge_InitializePlatformAudioSink(JNIEnv* env) {
  ::starboard::shared::starboard::audio_sink::SbAudioSinkImpl::Initialize();
}

void JNI_StarboardBridge_HandleDeepLink(JNIEnv* env,
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

void JNI_StarboardBridge_SetAndroidOSExperience(JNIEnv* env,
                                                jboolean isAmatiDevice) {
  std::string value = isAmatiDevice ? "Amati" : "Watson";
  auto header_value_provider =
      cobalt::browser::CobaltHeaderValueProvider::GetInstance();
  header_value_provider->SetHeaderValue(kAndroidOSExperienceHeader, value);
}

void JNI_StarboardBridge_SetAndroidPlayServicesVersion(JNIEnv* env,
                                                       jlong version) {
  auto header_value_provider =
      cobalt::browser::CobaltHeaderValueProvider::GetInstance();
  header_value_provider->SetHeaderValue(kPlayServicesVersionHeader,
                                        base::NumberToString(version));
}

void JNI_StarboardBridge_SetAndroidBuildFingerprint(
    JNIEnv* env,
    const JavaParamRef<jstring>& fingerprint) {
  auto header_value_provider =
      cobalt::browser::CobaltHeaderValueProvider::GetInstance();
  header_value_provider->SetHeaderValue(
      kBuildFingerprintHeader, ConvertJavaStringToUTF8(env, fingerprint));
}

void JNI_StarboardBridge_SetYoutubeCertificationScope(
    JNIEnv* env,
    const JavaParamRef<jstring>& certScope) {
  auto header_value_provider =
      cobalt::browser::CobaltHeaderValueProvider::GetInstance();
  header_value_provider->SetHeaderValue(
      kYoutubeCertScopeHeader, ConvertJavaStringToUTF8(env, certScope));
}

jboolean JNI_StarboardBridge_IsReleaseBuild(JNIEnv* env) {
#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  return true;
#else
  return false;
#endif
}

jboolean JNI_StarboardBridge_IsDevelopmentBuild(JNIEnv* env) {
// OFFICIAL_BUILD is set for Cobalt QA and Gold releases
#if defined(OFFICIAL_BUILD)
  return false;
#else
  return true;
#endif
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

bool StarboardBridge::IsPlatformErrorShowing(JNIEnv* env) {
  SB_DCHECK(env);
  return Java_StarboardBridge_isPlatformErrorShowing(env, j_starboard_bridge_);
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
  return limit_ad_tracking_java;
}

std::string StarboardBridge::GetUserAgentAuxField(JNIEnv* env) const {
  SB_DCHECK(env);
  return ConvertJavaStringToUTF8(
      env, Java_StarboardBridge_getUserAgentAuxField(env, j_starboard_bridge_));
}

bool StarboardBridge::IsAmatiDevice(JNIEnv* env) const {
  SB_DCHECK(env);
  return Java_StarboardBridge_getIsAmatiDevice(env, j_starboard_bridge_) ==
         JNI_TRUE;
}

std::string StarboardBridge::GetBuildFingerprint(JNIEnv* env) const {
  SB_DCHECK(env);
  return ConvertJavaStringToUTF8(
      env, Java_StarboardBridge_getBuildFingerprint(env, j_starboard_bridge_));
}

int64_t StarboardBridge::GetPlayServicesVersion(JNIEnv* env) const {
  SB_DCHECK(env);
  return static_cast<int64_t>(
      Java_StarboardBridge_getPlayServicesVersion(env, j_starboard_bridge_));
}

void StarboardBridge::CloseAllCobaltService(JNIEnv* env) const {
  SB_DCHECK(env);
  Java_StarboardBridge_closeAllCobaltService(env, j_starboard_bridge_);
}

void StarboardBridge::HideSplashScreen(JNIEnv* env) const {
  SB_DCHECK(env);
  Java_StarboardBridge_hideSplashScreen(env, j_starboard_bridge_);
}

void StarboardBridge::SetStartupMilestone(jint milestone) const {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_StarboardBridge_setStartupMilestone(env, j_starboard_bridge_, milestone);
}

}  // namespace starboard::android::shared
