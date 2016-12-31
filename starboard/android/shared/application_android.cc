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

#include <android_native_app_glue.h>
#include <time.h>

#include "starboard/android/shared/file_internal.h"
#include "starboard/android/shared/input_event.h"
#include "starboard/event.h"
#include "starboard/log.h"

namespace starboard {
namespace android {
namespace shared {

// "using" doesn't work with class members, so make a local convienience type.
typedef ::starboard::shared::starboard::Application::Event Event;

ApplicationAndroid::ApplicationAndroid(struct android_app* state)
  : android_state_(state), window_(kSbWindowInvalid) { }

ApplicationAndroid::~ApplicationAndroid() { }

void ApplicationAndroid::Initialize() {
  // Called once here to help SbTimeZoneGet*Name()
  tzset();
  SbFileAndroidInitialize(android_state_->activity);
}

void ApplicationAndroid::Teardown() {
  SbFileAndroidTeardown();
}

ANativeActivity* ApplicationAndroid::GetActivity() {
  return android_state_->activity;
}

SbWindow ApplicationAndroid::CreateWindow(const SbWindowOptions* options) {
  SB_UNREFERENCED_PARAMETER(options);
  // Don't allow re-creation since we only have one window anyway.
  if (SbWindowIsValid(window_)) {
    return kSbWindowInvalid;
  }

  // SbWindow, EGLNativeWindowType, and ANativeWindow* are all the same.
  window_ = reinterpret_cast<SbWindow>(android_state_->window);
  return window_;
}

bool ApplicationAndroid::DestroyWindow(SbWindow window) {
  if (!SbWindowIsValid(window)) {
    return false;
  }
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
  SB_LOG(INFO) << "OnAndroidCommand " << cmd;
  Event *event = NULL;

  // When an app first starts up the order of commands we get are:
  // 1. APP_CMD_START
  // 2. APP_CMD_RESUME
  // 3. APP_CMD_INPUT_CHANGED
  // 4. APP_CMD_INIT_WINDOW
  switch (cmd) {
    case APP_CMD_INIT_WINDOW:
      DispatchStart();
      break;
    case APP_CMD_START:
      SB_LOG(INFO) << "APP_CMD_START";
      if (state() != kStateUnstarted) {
        DispatchAndDelete(new Event(kSbEventTypeResume, NULL, NULL));
      }
      break;
    case APP_CMD_RESUME:
      SB_LOG(INFO) << "APP_CMD_RESUME";
      if (state() != kStateUnstarted) {
        DispatchAndDelete(new Event(kSbEventTypeUnpause, NULL, NULL));
      }
      break;
    case APP_CMD_PAUSE:
      SB_LOG(INFO) << "APP_CMD_PAUSE";
      DispatchAndDelete(new Event(kSbEventTypePause, NULL, NULL));
      break;
    case APP_CMD_STOP:
      SB_LOG(INFO) << "APP_CMD_STOP";
      DispatchAndDelete(new Event(kSbEventTypeSuspend, NULL, NULL));
      break;
    case APP_CMD_DESTROY:
      SB_LOG(INFO) << "APP_CMD_DESTROY";
      DispatchAndDelete(new Event(kSbEventTypeStop, NULL, NULL));
      break;
  }
}

bool ApplicationAndroid::OnAndroidInput(AInputEvent* androidEvent) {
  Event *event = CreateInputEvent(androidEvent, window_);
  if (event == NULL) {
    return false;
  }
  DispatchAndDelete(event);
  return true;
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, using the event
 * loop in ApplicationAndroid.
 */
extern "C" void android_main(struct android_app* state) {
  ApplicationAndroid application(state);
  state->userData = &application;
  state->onAppCmd = ApplicationAndroid::HandleCommand;
  state->onInputEvent = ApplicationAndroid::HandleInput;
  application.Run(0, NULL);
}

static SB_C_INLINE ApplicationAndroid* ToApplication(
    struct android_app* app) {
  return static_cast<ApplicationAndroid*>(app->userData);
}

// static
void ApplicationAndroid::HandleCommand(struct android_app* app, int32_t cmd) {
  SB_LOG(INFO) << "HandleCommand " << cmd;
  ToApplication(app)->OnAndroidCommand(cmd);
}

// static
int32_t ApplicationAndroid::HandleInput(
    struct android_app* app, AInputEvent* event) {
  return ToApplication(app)->OnAndroidInput(event) ? 1 : 0;
}

// TODO: Figure out how to export ANativeActivity_onCreate()
extern "C" SB_EXPORT_PLATFORM void CobaltActivity_onCreate(
    ANativeActivity *activity, void *savedState, size_t savedStateSize) {
  SB_LOG(INFO) << "CobaltActivity_onCreate";
  ANativeActivity_onCreate(activity, savedState, savedStateSize);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
