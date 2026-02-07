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

namespace starboard::android::shared {

using ::starboard::shared::starboard::Application;

// TODO(cobalt, b/378708359): Remove this dummy init.
void stubSbEventHandle(const SbEvent* event) {
  SB_LOG(ERROR) << "Starboard event DISCARDED:" << event->type;
}

ApplicationAndroid::ApplicationAndroid(
    std::unique_ptr<CommandLine> command_line,
    ScopedJavaGlobalRef<jobject> asset_manager,
    const std::string& files_dir,
    const std::string& cache_dir,
    const std::string& native_library_dir)
    : Application(stubSbEventHandle) {
  SB_LOG(INFO)
      << "COBALT_STARTUP_LOG: ApplicationAndroid::ApplicationAndroid [4/6]";
  SetCommandLine(std::move(command_line));
  // Initialize Time Zone early so that local time works correctly.
  // Called once here to help SbTimeZoneGet*Name()
  tzset();

  // Initialize Android asset access.
  SbFileAndroidInitialize(asset_manager, files_dir, cache_dir,
                          native_library_dir);

  // This effectively initializes the singleton and caches all RRO settings, if
  // they haven't yet be cached by other users of the RuntimeResourceOverlay
  // class.
  RuntimeResourceOverlay::GetInstance();

  JNIEnv* jni_env = base::android::AttachCurrentThread();
  app_start_timestamp_ = starboard_bridge_->GetAppStartTimestamp(jni_env);

  starboard_bridge_->ApplicationStarted(jni_env);
}

ApplicationAndroid::~ApplicationAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  starboard_bridge_->ApplicationStopping(env);

  // Detaches JNI, no more JNI calls after this.
  JniEnvExt::OnThreadShutdown();
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

Application::Event* ApplicationAndroid::GetNextEvent() {
  SB_LOG(FATAL) << __func__
                << " should not be called since Android doesn't utilize "
                   "Starboard's event handling";
  return nullptr;
}

void ApplicationAndroid::Inject(Application::Event* event) {
  SB_LOG(FATAL) << __func__
                << " should not be called since Android doesn't utilize "
                   "Starboard's event handling";
}

void ApplicationAndroid::InjectTimedEvent(
    Application::TimedEvent* timed_event) {
  SB_LOG(FATAL) << __func__
                << " should not be called since Android doesn't utilize "
                   "Starboard's event handling";
}

void ApplicationAndroid::CancelTimedEvent(SbEventId event_id) {
  SB_LOG(FATAL) << __func__
                << " should not be called since Android doesn't utilize "
                   "Starboard's event handling";
}

Application::TimedEvent* ApplicationAndroid::GetNextDueTimedEvent() {
  SB_LOG(FATAL) << __func__
                << " should not be called since Android doesn't utilize "
                   "Starboard's event handling";
  return nullptr;
}

int64_t ApplicationAndroid::GetNextTimedEventTargetTime() {
  SB_LOG(FATAL) << __func__
                << " should not be called since Android doesn't utilize "
                   "Starboard's event handling";
  return std::numeric_limits<int64_t>::max();
}

}  // namespace starboard::android::shared
