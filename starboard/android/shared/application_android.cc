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

#include <android/native_activity.h>
#include <android_native_app_glue.h>
#include <time.h>

#include <map>
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

namespace {

SbConditionVariable app_created_condition;
SbMutex app_created_mutex;

#ifndef NDEBUG
std::map<int, const char*> CreateAppCmdNamesMap() {
  std::map<int, const char*> m;
#define X(cmd) m[cmd] = #cmd
  X(APP_CMD_INPUT_CHANGED);
  X(APP_CMD_INIT_WINDOW);
  X(APP_CMD_TERM_WINDOW);
  X(APP_CMD_WINDOW_RESIZED);
  X(APP_CMD_WINDOW_REDRAW_NEEDED);
  X(APP_CMD_CONTENT_RECT_CHANGED);
  X(APP_CMD_GAINED_FOCUS);
  X(APP_CMD_LOST_FOCUS);
  X(APP_CMD_CONFIG_CHANGED);
  X(APP_CMD_LOW_MEMORY);
  X(APP_CMD_START);
  X(APP_CMD_RESUME);
  X(APP_CMD_SAVE_STATE);
  X(APP_CMD_PAUSE);
  X(APP_CMD_STOP);
  X(APP_CMD_DESTROY);
#undef X
  return m;
}
std::map<int, const char*> kAppCmdNames = CreateAppCmdNamesMap();
#endif  // NDEBUG

}  // namespace

namespace starboard {
namespace android {
namespace shared {

// "using" doesn't work with class members, so make a local convienience type.
typedef ::starboard::shared::starboard::Application::Event Event;

ApplicationAndroid::ApplicationAndroid(struct android_app* state)
    : android_state_(state),
      window_(kSbWindowInvalid),
      last_is_accessibility_high_contrast_text_enabled_(false),
      exit_on_destroy_(false),
      exit_error_level_(0) {}

ApplicationAndroid::~ApplicationAndroid() {}

void ApplicationAndroid::Initialize() {
  // Called once here to help SbTimeZoneGet*Name()
  tzset();
  SbFileAndroidInitialize(android_state_->activity);
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
  window_->native_window = android_state_->window;
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

bool ApplicationAndroid::DispatchNextEvent() {
  // We already dispatched our own system events in OnAndroidCommand() and/or
  // OnAndroidInput(), but we may have an injected event to dispatch.
  DispatchAndDelete(GetNextEvent());

  // Keep pumping events until Android is done with its lifecycle.
  return !android_state_->destroyRequested;
}

Event* ApplicationAndroid::WaitForSystemEventWithTimeout(SbTime time) {
  int ident;
  int looper_events;
  struct android_poll_source* source;
  int timeMillis = time / 1000;

  ident = ALooper_pollAll(timeMillis, NULL, &looper_events,
                          reinterpret_cast<void**>(&source));
  if (ident >= 0 && source != NULL) {
    // This will end up calling OnAndroidCommand() or OnAndroidInput().
    source->process(android_state_, source);
  }

  // Always return NULL since we already dispatched our own system events.
  return NULL;
}

void ApplicationAndroid::WakeSystemEventWait() {
  ALooper_wake(android_state_->looper);
}

void ApplicationAndroid::OnAndroidCommand(int32_t cmd) {
#ifndef NDEBUG
  const char* cmd_name = kAppCmdNames[cmd];
  if (cmd_name) {
    SB_DLOG(INFO) << cmd_name;
  } else {
    SB_DLOG(INFO) << "APP_CMD_[unknown  " << cmd << "]";
  }
#endif

  // The window surface being created/destroyed is more significant than the
  // Activity lifecycle since Cobalt can't do anything at all if it doesn't have
  // a window surface to draw on.
  switch (cmd) {
    case APP_CMD_INIT_WINDOW:
      if (window_) {
        window_->native_window = android_state_->window;
      }
      if (state() == kStateUnstarted) {
        // This is the initial launch, so we have to start Cobalt now that we
        // have a window.
        DispatchStart();
      } else {
        // Now that we got a window back, change the command for the switch
        // below to sync up with the current activity lifecycle.
        cmd = android_state_->activityState;
      }
      break;
    case APP_CMD_TERM_WINDOW:
      // Cobalt can't keep running without a window, even if the Activity hasn't
      // stopped yet. DispatchAndDelete() will inject events as needed if we're
      // not already paused.
      DispatchAndDelete(new Event(kSbEventTypeSuspend, NULL, NULL));
      if (window_) {
        window_->native_window = NULL;
      }
      break;
    case APP_CMD_GAINED_FOCUS: {
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
    case APP_CMD_DESTROY:
      // Calling exit() on Android is not party-line thinking.
      // However, if exit_on_destroy_ has been set, we're running
      // some sort of automated test that's completed. Terminating
      // the process is the only correct option.
      //
      // Note that we do not send kSbEventTypeStop if exit_on_destroy_
      // is not set. "Destroy" here means "activity destroy" not
      // "process destroy". Android can start a new activity in this
      // process again, and Starboard cannot exit it's stop state.
      if (exit_on_destroy_) {
        DispatchAndDelete(new Event(kSbEventTypeStop, NULL, NULL));
        // Note this log line may not flush before exit.
        // Even __android_log_close() does not seem to guarentee it.
        // Also __android_log_close() is not available on all platforms.
        SB_LOG(ERROR) << "***Stopping Application*** " << exit_error_level_;

        // The actual exit code here doesn't matter.
        exit(0);
      }
      break;
  }

  // If there's a window, sync the app state to the Activity lifecycle, letting
  // DispatchAndDelete() inject events as needed if we missed a state.
  if (android_state_->window) {
    switch (cmd) {
      case APP_CMD_START:
        DispatchAndDelete(new Event(kSbEventTypeResume, NULL, NULL));
        break;
      case APP_CMD_RESUME:
        DispatchAndDelete(new Event(kSbEventTypeUnpause, NULL, NULL));
        break;
      case APP_CMD_PAUSE:
        DispatchAndDelete(new Event(kSbEventTypePause, NULL, NULL));
        break;
      case APP_CMD_STOP:
        // In practice we've already suspended in APP_CMD_TERM_WINDOW above,
        // and DispatchAndDelete() will squelch this event.
        DispatchAndDelete(new Event(kSbEventTypeSuspend, NULL, NULL));
        break;
    }
  }
}

bool ApplicationAndroid::OnAndroidInput(AInputEvent* androidEvent) {
  SB_DCHECK(input_events_generator_);
  std::vector<Event*> events;
  bool handled =
      input_events_generator_->CreateInputEvents(androidEvent, &events);
  for (int i = 0; i < events.size(); ++i) {
    DispatchAndDelete(events[i]);
  }

  return handled;
}

ANativeActivity* ApplicationAndroid::GetActivity() {
  return android_state_->activity;
}

void ApplicationAndroid::SetExitOnActivityDestroy(int error_level) {
  exit_on_destroy_ = true;
  exit_error_level_ = error_level;
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
jboolean Java_foo_cobalt_coat_CobaltActivity_nativeOnSearchRequested() {
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
void Java_foo_cobalt_coat_CobaltActivity_nativeHandleDeepLink(
    JNIEnv* env, jobject unused_this, jstring j_url) {
  if (j_url) {
    const char* utf_chars = env->GetStringUTFChars(j_url, NULL);
    ApplicationAndroid::Get()->HandleDeepLink(utf_chars);
    env->ReleaseStringUTFChars(j_url, utf_chars);
  }
}

static std::string GetStartIntentUrl() {
  JniEnvExt* env = JniEnvExt::Get();
  std::string start_url;

  ScopedLocalJavaRef<jstring> j_url(env->CallActivityObjectMethodOrAbort(
      "getStartIntentUrl", "()Ljava/lang/String;"));
  if (j_url) {
    const char* utf_chars = env->GetStringUTFChars(j_url.Get(), NULL);
    start_url.assign(utf_chars);
    env->ReleaseStringUTFChars(j_url.Get(), utf_chars);
  }
  return start_url;
}

static void GetArgs(struct android_app* state, std::vector<char*>* out_args) {
  out_args->push_back(SbStringDuplicate("starboard"));

  JniEnvExt* env = JniEnvExt::Get();

  ScopedLocalJavaRef<jobjectArray> args_array(
      env->CallActivityObjectMethodOrAbort("getArgs", "()[Ljava/lang/String;"));
  jint argc = env->GetArrayLength(args_array.Get());

  for (jint i = 0; i < argc; i++) {
    ScopedLocalJavaRef<jstring> element(
        env->GetObjectArrayElementOrAbort(args_array.Get(), i));
    const char* utf_chars = env->GetStringUTFChars(element.Get(), NULL);
    out_args->push_back(SbStringDuplicate(utf_chars));
    env->ReleaseStringUTFChars(element.Get(), utf_chars);
  }
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, using the event
 * loop in ApplicationAndroid.
 */
extern "C" void android_main(struct android_app* state) {
  JniEnvExt::Initialize(state->activity);

  std::vector<char*> args;
  GetArgs(state, &args);

  ApplicationAndroid application(state);

  std::string start_url(GetStartIntentUrl());
  if (!start_url.empty()) {
    application.SetStartLink(start_url.c_str());
  }

  SbMutexAcquire(&app_created_mutex);
  SbConditionVariableSignal(&app_created_condition);
  SbMutexRelease(&app_created_mutex);

  state->userData = &application;
  state->onAppCmd = ApplicationAndroid::HandleCommand;
  state->onInputEvent = ApplicationAndroid::HandleInput;
  application.Run(args.size(), args.data());

  for (std::vector<char*>::iterator it = args.begin(); it != args.end(); ++it) {
    SbMemoryDeallocate(*it);
  }
}

static SB_C_INLINE ApplicationAndroid* ToApplication(
    struct android_app* app) {
  return static_cast<ApplicationAndroid*>(app->userData);
}

// static
void ApplicationAndroid::HandleCommand(struct android_app* app, int32_t cmd) {
  SB_DLOG(INFO) << "HandleCommand " << cmd;
  ToApplication(app)->OnAndroidCommand(cmd);
}

// static
int32_t ApplicationAndroid::HandleInput(
    struct android_app* app, AInputEvent* event) {
  return ToApplication(app)->OnAndroidInput(event) ? 1 : 0;
}

// This function exists for two reasons:
// (a) As our toolchain is currently configured, ANativeActivity_onCreate()
// is not an exported symbol. We export this instead.
// (b) We do some synchronization to ensure that Application::Get()
// will definitely work after this function returns.
extern "C" SB_EXPORT_PLATFORM void CobaltActivity_onCreate(
    ANativeActivity *activity, void *savedState, size_t savedStateSize) {
  SB_DLOG(INFO) << "CobaltActivity_onCreate";
  SbMutexCreate(&app_created_mutex);
  SbConditionVariableCreate(&app_created_condition, &app_created_mutex);
  SbMutexAcquire(&app_created_mutex);

  ANativeActivity_onCreate(activity, savedState, savedStateSize);

  SbConditionVariableWait(&app_created_condition, &app_created_mutex);
  SbMutexRelease(&app_created_mutex);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
