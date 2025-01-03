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

#include "starboard/android/shared/application_android.h"

#include <android/looper.h>
#include <android/native_activity.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "starboard/extension/accessibility.h"

#include "starboard/android/shared/file_internal.h"
#include "starboard/android/shared/input_events_generator.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/window_internal.h"
#include "starboard/common/condition_variable.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/event.h"
#include "starboard/key.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

namespace starboard {
namespace android {
namespace shared {

// TODO(cobalt, b/378708359): Remove this dummy init.
void stubSbEventHandle(const SbEvent* event) {
  SB_LOG(ERROR) << "Starboard event DISCARDED:" << event->type;
}

ApplicationAndroid::ApplicationAndroid(
    std::unique_ptr<CommandLine> command_line)
    : QueueApplication(stubSbEventHandle) {
  SetCommandLine(std::move(command_line));
  // Initialize Time Zone early so that local time works correctly.
  // Called once here to help SbTimeZoneGet*Name()
  tzset();

  // Initialize Android asset access early so that ICU can load its tables
  // from the assets. The use ICU is used in our logging.
  SbFileAndroidInitialize();

  JniEnvExt* env = JniEnvExt::Get();
  jobject local_ref = env->CallStarboardObjectMethodOrAbort(
      "getResourceOverlay", "()Ldev/cobalt/coat/ResourceOverlay;");
  resource_overlay_ = env->ConvertLocalRefToGlobalRef(local_ref);

  ::starboard::shared::starboard::audio_sink::SbAudioSinkImpl::Initialize();

  app_start_timestamp_ = starboard_bridge_->GetAppStartTimestamp();

  starboard_bridge_->ApplicationStarted();
}

ApplicationAndroid::~ApplicationAndroid() {
  starboard_bridge_->ApplicationStopping();

  // The application is exiting.
  // Release the global reference.
  if (resource_overlay_) {
    JniEnvExt* env = JniEnvExt::Get();
    env->DeleteGlobalRef(resource_overlay_);
    resource_overlay_ = nullptr;
  }
  // Detaches JNI, no more JNI calls after this.
  JniEnvExt::OnThreadShutdown();
}

extern "C" SB_EXPORT_PLATFORM jboolean
Java_dev_cobalt_coat_StarboardBridge_nativeOnSearchRequested(
    JniEnvExt* env,
    jobject unused_this) {
  // TODO(cobalt, b/378581064): how to handle onSearchRequested()?
  return true;
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_coat_CobaltSystemConfigChangeReceiver_nativeDateTimeConfigurationChanged(
    JNIEnv* env,
    jobject jcaller) {
  // TODO(cobalt, b/378705729): Make sure tzset() is called on the right thread.
  // Set the timezone to allow SbTimeZoneGetName() to return updated timezone.
  tzset();
}

extern "C" SB_EXPORT_PLATFORM jstring
Java_dev_cobalt_coat_javabridge_HTMLMediaElementExtension_nativeCanPlayType(
    JniEnvExt* env,
    jobject jcaller,
    jstring j_mime_type,
    jstring j_key_system) {
  std::string mime_type, key_system;
  if (j_mime_type) {
    mime_type = env->GetStringStandardUTFOrAbort(j_mime_type);
  }
  if (j_key_system) {
    key_system = env->GetStringStandardUTFOrAbort(j_key_system);
  }
  SbMediaSupportType support_type =
      SbMediaCanPlayMimeAndKeySystem(mime_type.c_str(), key_system.c_str());
  const char* ret;
  switch (support_type) {
    case kSbMediaSupportTypeNotSupported:
      ret = "";
      break;
    case kSbMediaSupportTypeMaybe:
      ret = "maybe";
      break;
    case kSbMediaSupportTypeProbably:
      ret = "probably";
      break;
  }
  SB_LOG(INFO) << __func__ << " (" << mime_type << ", " << key_system
               << ") --> " << ret;
  return env->NewStringStandardUTFOrAbort(ret);
}

int ApplicationAndroid::GetOverlayedIntValue(const char* var_name) {
  ScopedLock lock(overlay_mutex_);
  if (overlayed_int_variables_.find(var_name) !=
      overlayed_int_variables_.end()) {
    return overlayed_int_variables_[var_name];
  }
  JniEnvExt* env = JniEnvExt::Get();
  jint value = env->GetIntFieldOrAbort(resource_overlay_, var_name, "I");
  overlayed_int_variables_[var_name] = value;
  return value;
}

std::string ApplicationAndroid::GetOverlayedStringValue(const char* var_name) {
  ScopedLock lock(overlay_mutex_);
  if (overlayed_string_variables_.find(var_name) !=
      overlayed_string_variables_.end()) {
    return overlayed_string_variables_[var_name];
  }
  JniEnvExt* env = JniEnvExt::Get();
  std::string value = env->GetStringStandardUTFOrAbort(
      env->GetStringFieldOrAbort(resource_overlay_, var_name));
  overlayed_string_variables_[var_name] = value;
  return value;
}

bool ApplicationAndroid::GetOverlayedBoolValue(const char* var_name) {
  ScopedLock lock(overlay_mutex_);
  if (overlayed_bool_variables_.find(var_name) !=
      overlayed_bool_variables_.end()) {
    return overlayed_bool_variables_[var_name];
  }
  JniEnvExt* env = JniEnvExt::Get();
  jboolean value =
      env->GetBooleanFieldOrAbort(resource_overlay_, var_name, "Z");
  overlayed_bool_variables_[var_name] = value;
  return value;
}
}  // namespace shared
}  // namespace android
}  // namespace starboard
