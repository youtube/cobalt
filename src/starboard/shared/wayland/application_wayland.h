// Copyright 2016 Samsung Electronics. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WAYLAND_APPLICATION_WAYLAND_H_
#define STARBOARD_SHARED_WAYLAND_APPLICATION_WAYLAND_H_

#include <wayland-client.h>
#include <wayland-egl.h>
#include <tizen-extension-client-protocol.h>

#include "starboard/configuration.h"
#include "starboard/mutex.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/queue_application.h"
#include "starboard/types.h"
#include "starboard/window.h"
#include "starboard/input.h"

namespace starboard {
namespace shared {
namespace wayland {

class ApplicationWayland : public shared::starboard::QueueApplication {
 public:
  ApplicationWayland();
  ~ApplicationWayland() SB_OVERRIDE{};

  static ApplicationWayland* Get() {
    return static_cast<ApplicationWayland*>(
        shared::starboard::Application::Get());
  }

  // window
  SbWindow CreateWindow(const SbWindowOptions* options);
  bool DestroyWindow(SbWindow window);
  void SetCompositor(wl_compositor* compositor) { compositor_ = compositor; }
  wl_compositor* GetCompositor() { return compositor_; }
  void SetShell(wl_shell* shell) { shell_ = shell; }
  wl_shell* GetShell() { return shell_; }
  void SetPolicy(tizen_policy* policy) { tz_policy_ = policy; }
  tizen_policy* GetPolicy() { return tz_policy_; }
  void WindowRaise();

  // input devices
  void SetKeyboard(wl_keyboard* keyboard) { keyboard_ = keyboard; }
  wl_keyboard* GetKeyboard() { return keyboard_; }
  void SetSeat(wl_seat* seat) { seat_ = seat; }
  wl_seat* GetSeat() { return seat_; }

  // key event
  void CreateRepeatKey();
  void DeleteRepeatKey();
  void CreateKey(int key, int state, bool is_repeat);

  // state change
  void Pause() SB_OVERRIDE;
  void Unpause() SB_OVERRIDE;

  // state change observer
  class StateObserver {
   public:
    StateObserver() { ApplicationWayland::Get()->RegisterObserver(this); }
    ~StateObserver() { ApplicationWayland::Get()->UnregisterObserver(this); }
    virtual void OnAppPause() {}
    virtual void OnAppUnpause() {}
  };
  void RegisterObserver(StateObserver* observer);
  void UnregisterObserver(StateObserver* observer);

 protected:
  // --- Application overrides ---
  void Initialize() SB_OVERRIDE;
  void Teardown() SB_OVERRIDE;

  // --- QueueApplication overrides ---
  bool MayHaveSystemEvents() SB_OVERRIDE;
  Event* PollNextSystemEvent() SB_OVERRIDE;
  Event* WaitForSystemEventWithTimeout(SbTime time) SB_OVERRIDE;
  void WakeSystemEventWait() SB_OVERRIDE;

 private:
  // window
  SbWindow window_;
  wl_display* display_;
  wl_compositor* compositor_;
  wl_shell* shell_;
  tizen_policy* tz_policy_;

  // input devices
  wl_seat* seat_;
  wl_keyboard* keyboard_;
  int key_repeat_key_;
  int key_repeat_state_;
  SbEventId key_repeat_event_id_;
  SbTime key_repeat_interval_;

  // wakeup event
  int wakeup_fd_;

  // observers
  Mutex observers_mutex_;
  std::deque<StateObserver*> observers_;
};

}  // namespace wayland
}  // namespace tizen
}  // namespace starboard

#endif  // STARBOARD_SHARED_WAYLAND_APPLICATION_WAYLAND_H_
