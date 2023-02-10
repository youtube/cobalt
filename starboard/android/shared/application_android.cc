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
#include <string>
#include <vector>

#include "starboard/accessibility.h"
#include "starboard/android/shared/file_internal.h"
#include "starboard/android/shared/input_events_generator.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/window_internal.h"
#include "starboard/common/condition_variable.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/string.h"
#include "starboard/event.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

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
    case ApplicationAndroid::AndroidCommand::kNativeWindowCreated:
      return "NativeWindowCreated";
    case ApplicationAndroid::AndroidCommand::kNativeWindowDestroyed:
      return "NativeWindowDestroyed";
    case ApplicationAndroid::AndroidCommand::kWindowFocusGained:
      return "WindowFocusGained";
    case ApplicationAndroid::AndroidCommand::kWindowFocusLost:
      return "WindowFocusLost";
    case ApplicationAndroid::AndroidCommand::kDeepLink:
      return "DeepLink";
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
      android_command_readfd_(-1),
      android_command_writefd_(-1),
      keyboard_inject_readfd_(-1),
      keyboard_inject_writefd_(-1),
      android_command_condition_(android_command_mutex_),
      activity_state_(AndroidCommand::kUndefined),
      window_(kSbWindowInvalid),
      last_is_accessibility_high_contrast_text_enabled_(false) {
  // Initialize Time Zone early so that local time works correctly.
  // Called once here to help SbTimeZoneGet*Name()
  tzset();

  // Initialize Android asset access early so that ICU can load its tables
  // from the assets. The use ICU is used in our logging.
  SbFileAndroidInitialize();

  // Enable axes used by Cobalt.
  static const unsigned int required_axes[] = {
      AMOTION_EVENT_AXIS_Z,       AMOTION_EVENT_AXIS_RZ,
      AMOTION_EVENT_AXIS_HAT_X,   AMOTION_EVENT_AXIS_HAT_Y,
      AMOTION_EVENT_AXIS_HSCROLL, AMOTION_EVENT_AXIS_VSCROLL,
      AMOTION_EVENT_AXIS_WHEEL,
  };
  for (auto axis : required_axes) {
    GameActivityPointerAxes_enableAxis(axis);
  }

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

  JniEnvExt* env = JniEnvExt::Get();
  jobject local_ref = env->CallStarboardObjectMethodOrAbort(
      "getResourceOverlay", "()Ldev/cobalt/coat/ResourceOverlay;");
  resource_overlay_ = env->ConvertLocalRefToGlobalRef(local_ref);
}

ApplicationAndroid::~ApplicationAndroid() {
  // Release the global reference.
  if (resource_overlay_) {
    JniEnvExt* env = JniEnvExt::Get();
    env->DeleteGlobalRef(resource_overlay_);
    resource_overlay_ = nullptr;
  }
  ALooper_removeFd(looper_, android_command_readfd_);
  close(android_command_readfd_);
  close(android_command_writefd_);

  ALooper_removeFd(looper_, keyboard_inject_readfd_);
  close(keyboard_inject_readfd_);
  close(keyboard_inject_writefd_);
}

void ApplicationAndroid::Initialize() {
  SbAudioSinkPrivate::Initialize();
}

void ApplicationAndroid::Teardown() {
  SbAudioSinkPrivate::TearDown();
  SbFileAndroidTeardown();
}

SbWindow ApplicationAndroid::CreateWindow(const SbWindowOptions* options) {
  if (SbWindowIsValid(window_)) {
    return kSbWindowInvalid;
  }
  ScopedLock lock(input_mutex_);
  window_ = new SbWindowPrivate;
  window_->native_window = native_window_;
  input_events_generator_.reset(new InputEventsGenerator(window_));
  return window_;
}

bool ApplicationAndroid::DestroyWindow(SbWindow window) {
  if (!SbWindowIsValid(window)) {
    return false;
  }

  ScopedLock lock(input_mutex_);
  input_events_generator_.reset();

  SB_DCHECK(window == window_);
  delete window_;
  window_ = kSbWindowInvalid;
  return true;
}

Event* ApplicationAndroid::WaitForSystemEventWithTimeout(SbTime time) {
  // Limit the polling time in case some non-system event is injected.
  const int kMaxPollingTimeMillisecond = 10;

  // Convert from microseconds to milliseconds, taking the ceiling value.
  // If we take the floor, or round, then we end up busy looping every time
  // the next event time is less than one millisecond.
  int timeout_millis = (time + kSbTimeMillisecond - 1) / kSbTimeMillisecond;
  int looper_events;
  int ident = ALooper_pollAll(
      std::min(std::max(timeout_millis, 0), kMaxPollingTimeMillisecond), NULL,
      &looper_events, NULL);

  // Ignore new system events while processing one.
  handle_system_events_ = false;

  switch (ident) {
    case kLooperIdAndroidCommand:
      ProcessAndroidCommand();
      break;
    case kLooperIdKeyboardInject:
      ProcessKeyboardInject();
      break;
  }

  handle_system_events_ = true;

  // Always return NULL since we already dispatched our own system events.
  return NULL;
}

void ApplicationAndroid::WakeSystemEventWait() {
  ALooper_wake(looper_);
}

void ApplicationAndroid::OnResume() {
  JniEnvExt* env = JniEnvExt::Get();
  env->CallStarboardVoidMethodOrAbort("beforeStartOrResume", "()V");
}

void ApplicationAndroid::OnSuspend() {
  JniEnvExt* env = JniEnvExt::Get();
  env->CallStarboardVoidMethodOrAbort("beforeSuspend", "()V");
}

void ApplicationAndroid::StartMediaPlaybackService() {
  JniEnvExt* env = JniEnvExt::Get();
  env->CallStarboardVoidMethodOrAbort("startMediaPlaybackService", "()V");
}

void ApplicationAndroid::StopMediaPlaybackService() {
  JniEnvExt* env = JniEnvExt::Get();
  env->CallStarboardVoidMethodOrAbort("stopMediaPlaybackService", "()V");
}

void ApplicationAndroid::ProcessAndroidCommand() {
  JniEnvExt* env = JniEnvExt::Get();
  AndroidCommand cmd;
  int err = read(android_command_readfd_, &cmd, sizeof(cmd));
  SB_DCHECK(err >= 0) << "Command read failed. errno=" << errno;

  SB_LOG(INFO) << "Android command: " << AndroidCommandName(cmd.type);

  // The activity state to which we should sync the starboard state.
  AndroidCommand::CommandType sync_state = AndroidCommand::kUndefined;

  switch (cmd.type) {
    case AndroidCommand::kUndefined:
      break;
    // Starboard resume/suspend is tied to the UI window being created/destroyed
    // (rather than to the Activity lifecycle) since Cobalt can't do anything at
    // all if it doesn't have a window surface to draw on.
    case AndroidCommand::kNativeWindowCreated: {
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
        env->CallStarboardVoidMethodOrAbort("beforeStartOrResume", "()V");
#if SB_API_VERSION >= 13
        DispatchStart(GetAppStartTimestamp());
#else   // SB_API_VERSION >= 13
        DispatchStart();
#endif  // SB_API_VERSION >= 13
      } else {
        // Now that we got a window back, change the command for the switch
        // below to sync up with the current activity lifecycle.
        sync_state = activity_state_;
      }
      break;
    }
    case AndroidCommand::kNativeWindowDestroyed: {
      // No need to JNI call StarboardBridge.beforeSuspend() since we did it
      // early in SendAndroidCommand().
      {
        ScopedLock lock(android_command_mutex_);
// Cobalt can't keep running without a window, even if the Activity
// hasn't stopped yet. Block until conceal event has been processed.

// Only process injected events -- don't check system events since
// that may try to acquire the already-locked android_command_mutex_.
#if SB_API_VERSION >= 13
        InjectAndProcess(kSbEventTypeConceal, /* checkSystemEvents */ false);
#else
        InjectAndProcess(kSbEventTypeSuspend, /* checkSystemEvents */ false);
#endif

        if (window_) {
          window_->native_window = NULL;
        }
        native_window_ = NULL;
        // Now that we've suspended the Starboard app, and let go of the window,
        // signal that the Android UI thread can continue.
        android_command_condition_.Signal();
      }
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
      memset(&settings, 0, sizeof(settings));
      if (!SbAccessibilityGetDisplaySettings(&settings)) {
        break;
      }

      bool enabled = settings.has_high_contrast_text_setting &&
                     settings.is_high_contrast_text_enabled;

      if (enabled != last_is_accessibility_high_contrast_text_enabled_) {
        Inject(new Event(kSbEventTypeAccessibilitySettingsChanged, NULL, NULL));
      }
      last_is_accessibility_high_contrast_text_enabled_ = enabled;
      break;
    }

    // Remember the Android activity state to sync to when we have a window.
    case AndroidCommand::kStop:
      SbAtomicNoBarrier_Increment(&android_stop_count_, -1);
    // Intentional fall-through.
    case AndroidCommand::kStart:
    case AndroidCommand::kResume:
    case AndroidCommand::kPause:
      sync_state = activity_state_ = cmd.type;
      break;
    case AndroidCommand::kDeepLink: {
      char* deep_link = static_cast<char*>(cmd.data);
      SB_LOG(INFO) << "AndroidCommand::kDeepLink: deep_link=" << deep_link
                   << " state=" << state();
      if (deep_link != NULL) {
        if (state() == kStateUnstarted) {
          SetStartLink(deep_link);
          SB_LOG(INFO) << "ApplicationAndroid SetStartLink";
          SbMemoryDeallocate(static_cast<void*>(deep_link));
        } else {
          SB_LOG(INFO) << "ApplicationAndroid Inject: kSbEventTypeLink";
#if SB_API_VERSION >= 13
          Inject(new Event(kSbEventTypeLink, SbTimeGetMonotonicNow(), deep_link,
                           SbMemoryDeallocate));
#else   // SB_API_VERSION >= 13
          Inject(new Event(kSbEventTypeLink, deep_link, SbMemoryDeallocate));
#endif  // SB_API_VERSION >= 13
        }
      }
      break;
    }
  }

  // If there's an outstanding "stop" command, then don't update the app state
  // since it'll be overridden by the upcoming "stop" state.
  if (SbAtomicNoBarrier_Load(&android_stop_count_) > 0) {
    return;
  }

  // If there's a window, sync the app state to the Activity lifecycle.
  if (native_window_) {
    switch (sync_state) {
#if SB_API_VERSION >= 13
      case AndroidCommand::kStart:
        Inject(new Event(kSbEventTypeReveal, NULL, NULL));
        break;
      case AndroidCommand::kResume:
        Inject(new Event(kSbEventTypeFocus, NULL, NULL));
        break;
      case AndroidCommand::kPause:
        Inject(new Event(kSbEventTypeBlur, NULL, NULL));
        break;
      case AndroidCommand::kStop:
        Inject(new Event(kSbEventTypeConceal, NULL, NULL));
        break;
#else
      case AndroidCommand::kStart:
        Inject(new Event(kSbEventTypeResume, NULL, NULL));
        break;
      case AndroidCommand::kResume:
        Inject(new Event(kSbEventTypeUnpause, NULL, NULL));
        break;
      case AndroidCommand::kPause:
        Inject(new Event(kSbEventTypePause, NULL, NULL));
        break;
      case AndroidCommand::kStop:
        Inject(new Event(kSbEventTypeSuspend, NULL, NULL));
        break;
#endif
      default:
        break;
    }
  }
}

void ApplicationAndroid::SendAndroidCommand(AndroidCommand::CommandType type,
                                            void* data) {
  SB_LOG(INFO) << "Send Android command: " << AndroidCommandName(type);
  AndroidCommand cmd{type, data};
  ScopedLock lock(android_command_mutex_);
  write(android_command_writefd_, &cmd, sizeof(cmd));
  // Synchronization only necessary when managing resources.
  switch (type) {
    case AndroidCommand::kNativeWindowCreated:
    case AndroidCommand::kNativeWindowDestroyed:
      while (native_window_ != data) {
        android_command_condition_.Wait();
      }
      break;
    case AndroidCommand::kStop:
      SbAtomicNoBarrier_Increment(&android_stop_count_, 1);
      break;
    default:
      break;
  }
}

bool ApplicationAndroid::SendAndroidMotionEvent(
    const GameActivityMotionEvent* event) {
  bool result = false;

  ScopedLock lock(input_mutex_);
  if (!input_events_generator_) {
    return false;
  }

  // add motion event into the queue.
  InputEventsGenerator::Events app_events;
  result = input_events_generator_->CreateInputEventsFromGameActivityEvent(
      const_cast<GameActivityMotionEvent*>(event), &app_events);

  for (int i = 0; i < app_events.size(); ++i) {
    Inject(app_events[i].release());
  }

  return result;
}

bool ApplicationAndroid::SendAndroidKeyEvent(
    const GameActivityKeyEvent* event) {
  bool result = false;

#ifdef STARBOARD_INPUT_EVENTS_FILTER
  if (!input_events_filter_.ShouldProcessKeyEvent(event)) {
    return result;
  }
#endif

  ScopedLock lock(input_mutex_);
  if (!input_events_generator_) {
    return false;
  }

  // Add key event to the application queue.
  InputEventsGenerator::Events app_events;
  result = input_events_generator_->CreateInputEventsFromGameActivityEvent(
      const_cast<GameActivityKeyEvent*>(event), &app_events);
  for (int i = 0; i < app_events.size(); i++) {
    Inject(app_events[i].release());
  }

  return result;
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
Java_dev_cobalt_coat_KeyboardInputConnection_nativeHasOnScreenKeyboard(
    JniEnvExt* env,
    jobject unused_this) {
  return SbWindowOnScreenKeyboardIsSupported() ? JNI_TRUE : JNI_FALSE;
}

void ApplicationAndroid::SbWindowShowOnScreenKeyboard(SbWindow window,
                                                      const char* input_text,
                                                      int ticket) {
  JniEnvExt* env = JniEnvExt::Get();
  jobject j_keyboard_editor = env->CallStarboardObjectMethodOrAbort(
      "getKeyboardEditor", "()Ldev/cobalt/coat/KeyboardEditor;");
  env->CallVoidMethodOrAbort(j_keyboard_editor, "showKeyboard", "()V");
  int* data = new int;
  *data = ticket;
  Inject(new Event(kSbEventTypeOnScreenKeyboardShown, data,
                   &DeleteDestructor<int>));
  return;
}

void ApplicationAndroid::SbWindowHideOnScreenKeyboard(SbWindow window,
                                                      int ticket) {
  JniEnvExt* env = JniEnvExt::Get();
  jobject j_keyboard_editor = env->CallStarboardObjectMethodOrAbort(
      "getKeyboardEditor", "()Ldev/cobalt/coat/KeyboardEditor;");
  env->CallVoidMethodOrAbort(j_keyboard_editor, "hideKeyboard", "()V");
  int* data = new int;
  *data = ticket;
  Inject(new Event(kSbEventTypeOnScreenKeyboardHidden, data,
                   &DeleteDestructor<int>));
  return;
}

void ApplicationAndroid::SbWindowUpdateOnScreenKeyboardSuggestions(
    SbWindow window,
    const std::vector<std::string>& suggestions,
    int ticket) {
  JniEnvExt* env = JniEnvExt::Get();
  jobjectArray completions = env->NewObjectArray(
      suggestions.size(),
      env->FindClass("android/view/inputmethod/CompletionInfo"), 0);
  jstring str;
  jobject j_completion_info;
  for (size_t i = 0; i < suggestions.size(); i++) {
    str = env->NewStringUTF(suggestions[i].c_str());
    j_completion_info =
        env->NewObjectOrAbort("android/view/inputmethod/CompletionInfo",
                              "(JILjava/lang/CharSequence;)V", i, i, str);
    env->SetObjectArrayElement(completions, i, j_completion_info);
  }
  jobject j_keyboard_editor = env->CallStarboardObjectMethodOrAbort(
      "getKeyboardEditor", "()Ldev/cobalt/coat/KeyboardEditor;");
  env->CallVoidMethodOrAbort(j_keyboard_editor, "updateCustomCompletions",
                             "([Landroid/view/inputmethod/CompletionInfo;)V",
                             completions);
  int* data = new int;
  *data = ticket;
  Inject(new Event(kSbEventTypeOnScreenKeyboardSuggestionsUpdated, data,
                   &DeleteDestructor<int>));
  return;
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_coat_KeyboardInputConnection_nativeSendText(
    JniEnvExt* env,
    jobject unused_clazz,
    jstring text,
    jboolean is_composing) {
  if (text) {
    std::string utf_str = env->GetStringStandardUTFOrAbort(text);
    ApplicationAndroid::Get()->SbWindowSendInputEvent(utf_str.c_str(),
                                                      is_composing);
  }
}

void DeleteSbInputDataWithText(void* ptr) {
  SbInputData* data = static_cast<SbInputData*>(ptr);
  const char* input_text = data->input_text;
  data->input_text = NULL;
  delete input_text;
  ApplicationAndroid::DeleteDestructor<SbInputData>(ptr);
}

void ApplicationAndroid::SbWindowSendInputEvent(const char* input_text,
                                                bool is_composing) {
  char* text = SbStringDuplicate(input_text);
  SbInputData* data = new SbInputData();
  memset(data, 0, sizeof(*data));
  data->window = window_;
  data->type = kSbInputEventTypeInput;
  data->device_type = kSbInputDeviceTypeOnScreenKeyboard;
  data->input_text = text;
  data->is_composing = is_composing;
  Inject(new Event(kSbEventTypeInput, data, &DeleteSbInputDataWithText));
  return;
}

bool ApplicationAndroid::OnSearchRequested() {
  for (int i = 0; i < 2; i++) {
    SbInputData* data = new SbInputData();
    memset(data, 0, sizeof(*data));
    data->window = window_;
    data->key = kSbKeyBrowserSearch;
    data->type = (i == 0) ? kSbInputEventTypePress : kSbInputEventTypeUnpress;
    Inject(new Event(kSbEventTypeInput, data, &DeleteDestructor<SbInputData>));
  }
  return true;
}

extern "C" SB_EXPORT_PLATFORM jboolean
Java_dev_cobalt_coat_StarboardBridge_nativeOnSearchRequested(
    JniEnvExt* env,
    jobject unused_this) {
  return ApplicationAndroid::Get()->OnSearchRequested();
}

void ApplicationAndroid::HandleDeepLink(const char* link_url) {
  SB_LOG(INFO) << "ApplicationAndroid::HandleDeepLink link_url=" << link_url;
  if (link_url == NULL || link_url[0] == '\0') {
    return;
  }
  char* deep_link = SbStringDuplicate(link_url);
  SB_DCHECK(deep_link);

  SendAndroidCommand(AndroidCommand::kDeepLink, deep_link);
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_coat_StarboardBridge_nativeHandleDeepLink(JniEnvExt* env,
                                                          jobject unused_this,
                                                          jstring j_url) {
  if (j_url) {
    std::string utf_str = env->GetStringStandardUTFOrAbort(j_url);
    ApplicationAndroid::Get()->HandleDeepLink(utf_str.c_str());
  }
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_coat_StarboardBridge_nativeStopApp(JniEnvExt* env,
                                                   jobject unused_this,
                                                   jint error_level) {
  ApplicationAndroid::Get()->Stop(error_level);
}

void ApplicationAndroid::SendLowMemoryEvent() {
  Inject(new Event(kSbEventTypeLowMemory, NULL, NULL));
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_coat_CobaltActivity_nativeLowMemoryEvent(JNIEnv* env,
                                                         jobject unused_clazz) {
  ApplicationAndroid::Get()->SendLowMemoryEvent();
}

void ApplicationAndroid::OsNetworkStatusChange(bool became_online) {
  if (state() == kStateUnstarted) {
    // Injecting events before application starts is error-prone.
    return;
  }
  if (became_online) {
    Inject(new Event(kSbEventTypeOsNetworkConnected, NULL, NULL));
  } else {
    Inject(new Event(kSbEventTypeOsNetworkDisconnected, NULL, NULL));
  }
}

SbTimeMonotonic ApplicationAndroid::GetAppStartTimestamp() {
  JniEnvExt* env = JniEnvExt::Get();
  jlong app_start_timestamp =
      env->CallStarboardLongMethodOrAbort("getAppStartTimestamp", "()J");
  return app_start_timestamp;
}

extern "C" SB_EXPORT_PLATFORM jlong
Java_dev_cobalt_coat_StarboardBridge_nativeSbTimeGetMonotonicNow(
    JNIEnv* env,
    jobject jcaller,
    jboolean online) {
  return SbTimeGetMonotonicNow();
}

void ApplicationAndroid::SendDateTimeConfigurationChangedEvent() {
  // Set the timezone to allow SbTimeZoneGetName() to return updated timezone.
  tzset();
  Inject(new Event(kSbEventDateTimeConfigurationChanged, NULL, NULL));
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_coat_CobaltSystemConfigChangeReceiver_nativeDateTimeConfigurationChanged(
    JNIEnv* env,
    jobject jcaller) {
  ApplicationAndroid::Get()->SendDateTimeConfigurationChangedEvent();
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
