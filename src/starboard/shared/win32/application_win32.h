// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WIN32_APPLICATION_WIN32_H_
#define STARBOARD_SHARED_WIN32_APPLICATION_WIN32_H_

// Windows headers.
#include <windows.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "starboard/mutex.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/localized_strings.h"
#include "starboard/shared/starboard/queue_application.h"
#include "starboard/shared/win32/window_internal.h"
#include "starboard/system.h"
#include "starboard/window.h"

namespace starboard {
namespace shared {
namespace win32 {

class ApplicationWin32 : public starboard::QueueApplication {
 public:
  ApplicationWin32();
  ~ApplicationWin32() override;

  static ApplicationWin32* Get() {
    return static_cast<ApplicationWin32*>(
        ::starboard::shared::starboard::Application::Get());
  }

  SbWindow CreateWindowForWin32(const SbWindowOptions* options);

  bool DestroyWindow(SbWindow window);

  void DispatchStart() { starboard::Application::DispatchStart(); }

  SbWindow GetCoreWindow() {
    return SbWindowIsValid(window_.get()) ? window_.get() : kSbWindowInvalid;
  }

  SbSystemPlatformError OnSbSystemRaisePlatformError(
      SbSystemPlatformErrorType type,
      SbSystemPlatformErrorCallback callback,
      void* user_data);

  void OnSbSystemClearPlatformError(SbSystemPlatformError handle);

  std::string GetLocalizedString(const char* id, const char* fallback) const {
    return localized_strings_.GetString(id, fallback);
  }

  // Returns true if it is valid to poll/query for system events.
  bool MayHaveSystemEvents() override { return true; }

  // Waits for an event until the timeout |time| runs out.  If an event occurs
  // in this time, it is returned, otherwise NULL is returned. If |time| is zero
  // or negative, then this should function effectively like a no-wait poll.
  Event* WaitForSystemEventWithTimeout(SbTime time) override;

  // Wakes up any thread waiting within a call to
  // WaitForSystemEventWithTimeout().
  void WakeSystemEventWait() override {
    ScopedLock lock(stop_waiting_for_system_events_mutex_);
    stop_waiting_for_system_events_ = true;
  }

  LRESULT WindowProcess(HWND hWnd, UINT msg, WPARAM w_param, LPARAM l_param);
  VOID TimedEventCallback(PVOID lp, BOOLEAN timer_or_wait_fired);

  // Non-virtual override. Calls into super class Run().
  int Run(int argc, char** argv);

 private:
  // --- Application overrides ---
  bool IsStartImmediate() override { return true; }
  void Initialize() override {}
  void Teardown() override {}

  void ProcessNextSystemMessage();
  SbTimeMonotonic GetNextTimedEventTargetTime() override {
    return SbTimeGetMonotonicNow();
  }

  // Processes window key events, returning a corresponding Event instance.
  // This transfers ownership of the returned Event.
  Event* ProcessWinKeyEvent(SbWindow window,
                            UINT msg,
                            WPARAM w_param,
                            LPARAM l_param);

  // Processes window mouse events, returning a corresponding Event instance.
  // This transfers ownership of the returned Event. The Event may be nullptr.
  Event* ProcessWinMouseEvent(SbWindow window,
                              UINT msg,
                              WPARAM w_param,
                              LPARAM l_param);

  Event* pending_event_ = nullptr;

  // The single open window, if any.
  std::unique_ptr<SbWindowPrivate> window_;

  starboard::LocalizedStrings localized_strings_;

  Mutex stop_waiting_for_system_events_mutex_;
  bool stop_waiting_for_system_events_;

  // The current depressed SbKeyModifiers - if there are any.
  unsigned int current_key_modifiers_ = 0;
};

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_APPLICATION_WIN32_H_
