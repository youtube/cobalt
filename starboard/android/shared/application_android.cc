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

namespace {
// Only needed for Cobalt's TTS.
constexpr int kLooperIdKeyboardInject = 0;
}  // namespace

// "using" doesn't work with class members, so make a local convenience type.
typedef ::starboard::shared::starboard::Application::Event Event;

// TODO(cobalt, b/378708359): Remove this dummy init.
void stubSbEventHandle(const SbEvent* event) {
  SB_LOG(ERROR) << "Starboard event DISCARDED:" << event->type;
}

ApplicationAndroid::ApplicationAndroid(
    ALooper* looper,
    std::unique_ptr<CommandLine> command_line)
    : QueueApplication(stubSbEventHandle),
      looper_(looper),
      keyboard_inject_readfd_(-1),
      keyboard_inject_writefd_(-1) {
  SetCommandLine(std::move(command_line));
  // Initialize Time Zone early so that local time works correctly.
  // Called once here to help SbTimeZoneGet*Name()
  tzset();

  // Initialize Android asset access early so that ICU can load its tables
  // from the assets. The use ICU is used in our logging.
  SbFileAndroidInitialize();

  int pipefd[2];
  int err;
  err = pipe(pipefd);
  SB_CHECK(err >= 0) << "pipe errno is:" << errno;
  keyboard_inject_readfd_ = pipefd[0];
  keyboard_inject_writefd_ = pipefd[1];
  ALooper_addFd(looper_, keyboard_inject_readfd_, kLooperIdKeyboardInject,
                ALOOPER_EVENT_INPUT, NULL, NULL);

  JniEnvExt* env = JniEnvExt::Get();
  jobject local_ref = env->CallStarboardObjectMethodOrAbort(
      "getResourceOverlay", "()Ldev/cobalt/coat/ResourceOverlay;");
  resource_overlay_ = env->ConvertLocalRefToGlobalRef(local_ref);

  ::starboard::shared::starboard::audio_sink::SbAudioSinkImpl::Initialize();

  JNIEnv* jni_env = base::android::AttachCurrentThread();
  app_start_timestamp_ = starboard_bridge_->GetAppStartTimestamp(jni_env);

  starboard_bridge_->ApplicationStarted(jni_env);
}

ApplicationAndroid::~ApplicationAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  starboard_bridge_->ApplicationStopping(env);

  ALooper_removeFd(looper_, keyboard_inject_readfd_);
  close(keyboard_inject_readfd_);
  close(keyboard_inject_writefd_);

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

Event* ApplicationAndroid::WaitForSystemEventWithTimeout(int64_t time) {
  // Limit the polling time in case some non-system event is injected.
  const int kMaxPollingTimeMillisecond = 1000;

  // Convert from microseconds to milliseconds, taking the ceiling value.
  // If we take the floor, or round, then we end up busy looping every time
  // the next event time is less than one millisecond.
  int timeout_millis =
      (time < std::min(std::numeric_limits<int64_t>::max() - 1000,
                       1000 * static_cast<int64_t>(INT_MAX - 1)))
          ? (time + 1000 - 1) / 1000
          : INT_MAX;
  int looper_events;
  int ident = ALooper_pollOnce(
      std::min(std::max(timeout_millis, 0), kMaxPollingTimeMillisecond), NULL,
      &looper_events, NULL);

  // Ignore new system events while processing one.
  handle_system_events_.store(false);

  switch (ident) {
    case kLooperIdKeyboardInject:
      ProcessKeyboardInject();
      break;
  }

  handle_system_events_.store(true);

  // Always return NULL since we already dispatched our own system events.
  return NULL;
}

void ApplicationAndroid::WakeSystemEventWait() {
  ALooper_wake(looper_);
}

void ApplicationAndroid::ProcessKeyboardInject() {
  SbKey key;
  int err = read(keyboard_inject_readfd_, &key, sizeof(key));
  SB_DCHECK(err >= 0) << "Keyboard inject read failed: errno=" << errno;
  SB_LOG(INFO) << "Keyboard inject: " << key;
  ScopedLock lock(input_mutex_);
  if (!input_events_generator_) {
    SB_DLOG(WARNING) << "Injected input event ignored without an SbWindow.";
    return;
  }
  InputEventsGenerator::Events app_events;
  input_events_generator_->CreateInputEventsFromSbKey(key, &app_events);
  for (int i = 0; i < app_events.size(); ++i) {
    Inject(app_events[i].release());
  }
}

void ApplicationAndroid::SendKeyboardInject(SbKey key) {
  write(keyboard_inject_writefd_, &key, sizeof(key));
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_coat_CobaltA11yHelper_nativeInjectKeyEvent(JNIEnv* env,
                                                           jobject unused_clazz,
                                                           jint key) {
  ApplicationAndroid::Get()->SendKeyboardInject(static_cast<SbKey>(key));
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
