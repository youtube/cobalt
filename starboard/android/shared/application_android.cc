// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <string>
#include <vector>

#include "starboard/accessibility.h"
#include "starboard/android/shared/file_internal.h"
#include "starboard/android/shared/input_events_generator.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/window_internal.h"
#include "starboard/condition_variable.h"
#include "starboard/event.h"
#include "starboard/log.h"
#include "starboard/mutex.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/string.h"

namespace starboard {
namespace android {
namespace shared {

namespace {
  enum {
    kLooperIdAndroidCommand,
    kLooperIdAndroidInput,
    kLooperIdKeyboardInject,
  };

  const char* AndroidCommandName(
      ApplicationAndroid::AndroidCommand::CommandType type) {
    switch (type) {
      case ApplicationAndroid::AndroidCommand::kUndefined:
        return "Undefined";
      case ApplicationAndroid::AndroidCommand::kStart:
        return "Start";
      case ApplicationAndroid::AndroidCommand::kResume:
        return "Resume";
      case ApplicationAndroid::AndroidCommand::kPause:
        return "Pause";
      case ApplicationAndroid::AndroidCommand::kStop:
        return "Stop";
      case ApplicationAndroid::AndroidCommand::kInputQueueChanged:
        return "InputQueueChanged";
      case ApplicationAndroid::AndroidCommand::kNativeWindowCreated:
        return "NativeWindowCreated";
      case ApplicationAndroid::AndroidCommand::kNativeWindowDestroyed:
        return "NativeWindowDestroyed";
      case ApplicationAndroid::AndroidCommand::kWindowFocusGained:
        return "WindowFocusGained";
      case ApplicationAndroid::AndroidCommand::kWindowFocusLost:
        return "WindowFocusLost";
      default:
        return "unknown";
    }
  }
}  // namespace

// "using" doesn't work with class members, so make a local convenience type.
typedef ::starboard::shared::starboard::Application::Event Event;

ApplicationAndroid::ApplicationAndroid(ALooper* looper)
    : looper_(looper),
      native_window_(NULL),
      input_queue_(NULL),
      android_command_readfd_(-1),
      android_command_writefd_(-1),
      keyboard_inject_readfd_(-1),
      keyboard_inject_writefd_(-1),
      android_command_condition_(android_command_mutex_),
      activity_state_(AndroidCommand::kUndefined),
      window_(kSbWindowInvalid),
      last_is_accessibility_high_contrast_text_enabled_(false) {
  int pipefd[2];
  int err;

  err = pipe(pipefd);
  SB_CHECK(err >= 0) << "pipe errno is:" << errno;
  android_command_readfd_ = pipefd[0];
  android_command_writefd_ = pipefd[1];
  ALooper_addFd(looper_, android_command_readfd_, kLooperIdAndroidCommand,
                ALOOPER_EVENT_INPUT, NULL, NULL);

  err = pipe(pipefd);
  SB_CHECK(err >= 0) << "pipe errno is:" << errno;
  keyboard_inject_readfd_ = pipefd[0];
  keyboard_inject_writefd_ = pipefd[1];
  ALooper_addFd(looper_, keyboard_inject_readfd_, kLooperIdKeyboardInject,
                ALOOPER_EVENT_INPUT, NULL, NULL);
}

ApplicationAndroid::~ApplicationAndroid() {
  ALooper_removeFd(looper_, android_command_readfd_);
  close(android_command_readfd_);
  close(android_command_writefd_);

  ALooper_removeFd(looper_, keyboard_inject_readfd_);
  close(keyboard_inject_readfd_);
  close(keyboard_inject_writefd_);
}

void ApplicationAndroid::Initialize() {
  // Called once here to help SbTimeZoneGet*Name()
  tzset();
  SbFileAndroidInitialize();
  SbAudioSinkPrivate::Initialize();
}

void ApplicationAndroid::Teardown() {
  SbAudioSinkPrivate::TearDown();
  SbFileAndroidTeardown();
}

SbWindow ApplicationAndroid::CreateWindow(const SbWindowOptions* options) {
  SB_UNREFERENCED_PARAMETER(options);
  if (SbWindowIsValid(window_)) {
    return kSbWindowInvalid;
  }
  window_ = new SbWindowPrivate;
  window_->native_window = native_window_;
  input_events_generator_.reset(new InputEventsGenerator(window_));
  return window_;
}

bool ApplicationAndroid::DestroyWindow(SbWindow window) {
  if (!SbWindowIsValid(window)) {
    return false;
  }

  input_events_generator_.reset();

  SB_DCHECK(window == window_);
  delete window_;
  window_ = kSbWindowInvalid;
  return true;
}

Event* ApplicationAndroid::WaitForSystemEventWithTimeout(SbTime time) {
  // Convert from microseconds to milliseconds, taking the ceiling value.
  // If we take the floor, or round, then we end up busy looping every time
  // the next event time is less than one millisecond.
  int timeout_millis = (time + kSbTimeMillisecond - 1) / kSbTimeMillisecond;
  int looper_events;
  int ident = ALooper_pollAll(timeout_millis, NULL, &looper_events, NULL);
  switch (ident) {
    case kLooperIdAndroidCommand:
      ProcessAndroidCommand();
      break;
    case kLooperIdAndroidInput:
      ProcessAndroidInput();
      break;
    case kLooperIdKeyboardInject:
      ProcessKeyboardInject();
      break;
  }

  // Always return NULL since we already dispatched our own system events.
  return NULL;
}

void ApplicationAndroid::WakeSystemEventWait() {
  ALooper_wake(looper_);
}

void ApplicationAndroid::ProcessAndroidCommand() {
  AndroidCommand cmd;
  int err = read(android_command_readfd_, &cmd, sizeof(cmd));
  SB_DCHECK(err >= 0) << "Command read failed. errno=" << errno;

  SB_LOG(INFO) << "Android command: " << AndroidCommandName(cmd.type);

  // The activity state to which we should sync the starboard state.
  AndroidCommand::CommandType sync_state = AndroidCommand::kUndefined;

  switch (cmd.type) {
    case AndroidCommand::kUndefined:
      break;

    case AndroidCommand::kInputQueueChanged: {
      ScopedLock lock(android_command_mutex_);
      if (input_queue_) {
        AInputQueue_detachLooper(input_queue_);
      }
      input_queue_ = static_cast<AInputQueue*>(cmd.data);
      if (input_queue_) {
        AInputQueue_attachLooper(input_queue_, looper_, kLooperIdAndroidInput,
                                 NULL, NULL);
      }
      // Now that we've swapped our use of the input queue, signal that the
      // Android UI thread can continue.
      android_command_condition_.Signal();
      break;
    }

    // The window surface being created/destroyed is more significant than the
    // Activity lifecycle since Cobalt can't do anything at all if it doesn't
    // have a window surface to draw on.
    case AndroidCommand::kNativeWindowCreated:
      {
        ScopedLock lock(android_command_mutex_);
        native_window_ = static_cast<ANativeWindow*>(cmd.data);
        if (window_) {
          window_->native_window = native_window_;
        }
        // Now that we have the window, signal that the Android UI thread can
        // continue, before we start or resume the Starboard app.
        android_command_condition_.Signal();
      }
      if (state() == kStateUnstarted) {
        // This is the initial launch, so we have to start Cobalt now that we
        // have a window.
        DispatchStart();
      } else {
        // Now that we got a window back, change the command for the switch
        // below to sync up with the current activity lifecycle.
        sync_state = activity_state_;
      }
      break;
    case AndroidCommand::kNativeWindowDestroyed: {
      ScopedLock lock(android_command_mutex_);
      // Cobalt can't keep running without a window, even if the Activity hasn't
      // stopped yet. DispatchAndDelete() will inject events as needed if we're
      // not already paused.
      DispatchAndDelete(new Event(kSbEventTypeSuspend, NULL, NULL));
      if (window_) {
        window_->native_window = NULL;
      }
      native_window_ = NULL;
      // Now that we've suspended the Starboard app, and let go of the window,
      // signal that the Android UI thread can continue.
      android_command_condition_.Signal();
      break;
    }

    case AndroidCommand::kWindowFocusLost:
      break;
    case AndroidCommand::kWindowFocusGained: {
      // Android does not have a publicly-exposed way to
      // register for high-contrast text settings changed events.
      // We assume that it can only change when our focus changes
      // (because the user exits and enters the app) so we check
      // for changes here.
      SbAccessibilityDisplaySettings settings;
      SbMemorySet(&settings, 0, sizeof(settings));
      if (!SbAccessibilityGetDisplaySettings(&settings)) {
        break;
      }

      bool enabled = settings.has_high_contrast_text_setting &&
          settings.is_high_contrast_text_enabled;

      if (enabled != last_is_accessibility_high_contrast_text_enabled_) {
        DispatchAndDelete(new Event(
            kSbEventTypeAccessiblitySettingsChanged, NULL, NULL));
      }
      last_is_accessibility_high_contrast_text_enabled_ = enabled;
      break;
    }

    // Remember the Android activity state to sync to when we have a window.
    case AndroidCommand::kStart:
    case AndroidCommand::kResume:
    case AndroidCommand::kPause:
    case AndroidCommand::kStop:
      sync_state = activity_state_ = cmd.type;
      break;
  }

  // If there's a window, sync the app state to the Activity lifecycle, letting
  // DispatchAndDelete() inject events as needed if we missed a state.
  if (native_window_) {
    switch (sync_state) {
      case AndroidCommand::kStart:
        DispatchAndDelete(new Event(kSbEventTypeResume, NULL, NULL));
        break;
      case AndroidCommand::kResume:
        DispatchAndDelete(new Event(kSbEventTypeUnpause, NULL, NULL));
        break;
      case AndroidCommand::kPause:
        DispatchAndDelete(new Event(kSbEventTypePause, NULL, NULL));
        break;
      case AndroidCommand::kStop:
        // In practice we've already suspended in kNativeWindowDestroyed
        // above, so DispatchAndDelete() will squelch this event.
        DispatchAndDelete(new Event(kSbEventTypeSuspend, NULL, NULL));
        break;
      default:
        break;
    }
  }
}

void ApplicationAndroid::SendAndroidCommand(AndroidCommand::CommandType type,
                                            void* data) {
  SB_LOG(INFO) << "Send Android command: " << AndroidCommandName(type);
  AndroidCommand cmd {type, data};
  ScopedLock lock(android_command_mutex_);
  write(android_command_writefd_, &cmd, sizeof(cmd));
  // Synchronization only necessary when managing resources.
  switch (type) {
    case AndroidCommand::kInputQueueChanged:
      while (input_queue_ != data) {
        android_command_condition_.Wait();
      }
      break;
    case AndroidCommand::kNativeWindowCreated:
    case AndroidCommand::kNativeWindowDestroyed:
      while (native_window_ != data) {
        android_command_condition_.Wait();
      }
      break;
    default:
      break;
  }
}

void ApplicationAndroid::ProcessAndroidInput() {
  SB_DCHECK(input_events_generator_);
  AInputEvent* android_event = NULL;
  while (AInputQueue_getEvent(input_queue_, &android_event) >= 0) {
    SB_LOG(INFO) << "Android input: type="
                 << AInputEvent_getType(android_event);
    if (AInputQueue_preDispatchEvent(input_queue_, android_event)) {
        continue;
    }
    InputEventsGenerator::Events app_events;
    bool handled = input_events_generator_->CreateInputEventsFromAndroidEvent(
        android_event, &app_events);
    for (int i = 0; i < app_events.size(); ++i) {
      DispatchAndDelete(app_events[i].release());
    }
    AInputQueue_finishEvent(input_queue_, android_event, handled);
  }
}

void ApplicationAndroid::ProcessKeyboardInject() {
  SbKey key;
  int err = read(keyboard_inject_readfd_, &key, sizeof(key));
  SB_DCHECK(err >= 0) << "Keyboard inject read failed: errno=" << errno;
  SB_LOG(INFO) << "Keyboard inject: " << key;

  InputEventsGenerator::Events app_events;
  input_events_generator_->CreateInputEventsFromSbKey(key, &app_events);
  for (int i = 0; i < app_events.size(); ++i) {
    DispatchAndDelete(app_events[i].release());
  }
}

void ApplicationAndroid::SendKeyboardInject(SbKey key) {
  write(keyboard_inject_writefd_, &key, sizeof(key));
}

extern "C" SB_EXPORT_PLATFORM void
Java_foo_cobalt_coat_CobaltA11yHelper_injectKeyEvent(JNIEnv* env,
                                                     jobject unused_clazz,
                                                     jint key) {
  ApplicationAndroid::Get()->SendKeyboardInject(static_cast<SbKey>(key));
}

bool ApplicationAndroid::OnSearchRequested() {
  for (int i = 0; i < 2; i++) {
    SbInputData* data = new SbInputData();
    SbMemorySet(data, 0, sizeof(*data));
    data->window = window_;
    data->key = kSbKeyBrowserSearch;
    data->type = (i == 0) ? kSbInputEventTypePress : kSbInputEventTypeUnpress;
    Inject(new Event(kSbEventTypeInput, data, &DeleteDestructor<SbInputData>));
  }
  return true;
}

extern "C" SB_EXPORT_PLATFORM
jboolean Java_foo_cobalt_coat_StarboardBridge_nativeOnSearchRequested(
    JniEnvExt* env, jobject unused_this) {
  return ApplicationAndroid::Get()->OnSearchRequested();
}

void ApplicationAndroid::HandleDeepLink(const char* link_url) {
  if (link_url == NULL || link_url[0] == '\0') {
    return;
  }
  char* deep_link = SbStringDuplicate(link_url);
  SB_DCHECK(deep_link);
  Inject(new Event(kSbEventTypeLink, deep_link, SbMemoryDeallocate));
}

extern "C" SB_EXPORT_PLATFORM
void Java_foo_cobalt_coat_StarboardBridge_nativeHandleDeepLink(
    JniEnvExt* env, jobject unused_this, jstring j_url) {
  if (j_url) {
    std::string utf_str = env->GetStringStandardUTFOrAbort(j_url);
    ApplicationAndroid::Get()->HandleDeepLink(utf_str.c_str());
  }
}

extern "C" SB_EXPORT_PLATFORM
void Java_foo_cobalt_coat_StarboardBridge_nativeStopApp(
    JniEnvExt* env, jobject unused_this, jint error_level) {
  ApplicationAndroid::Get()->Stop(error_level);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
