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

#ifndef STARBOARD_SHARED_WAYLAND_APPLICATION_WAYLAND_H_
#define STARBOARD_SHARED_WAYLAND_APPLICATION_WAYLAND_H_

#include <wayland-client.h>
#include <wayland-egl.h>

#include <deque>

#include "starboard/common/mutex.h"
#include "starboard/configuration.h"
#include "starboard/event.h"
#include "starboard/input.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/queue_application.h"
#include "starboard/shared/wayland/dev_input.h"
#include "starboard/types.h"
#include "starboard/window.h"

namespace starboard {
namespace shared {
namespace wayland {

class ApplicationWayland : public shared::starboard::QueueApplication {
 public:
  explicit ApplicationWayland(float video_pixel_ratio = 1.0f);
  ~ApplicationWayland() override{};

  static ApplicationWayland* Get() {
    return static_cast<ApplicationWayland*>(
        shared::starboard::Application::Get());
  }

  // wl registry add listener
  virtual bool OnGlobalObjectAvailable(struct wl_registry* registry,
                                       uint32_t name,
                                       const char* interface,
                                       uint32_t version);

  // display
  wl_display* GetWLDisplay() { return display_; }

  // window
  virtual SbWindow CreateWindow(const SbWindowOptions* options);
  bool DestroyWindow(SbWindow window);

  // state change
  void Pause(void* context, EventHandledCallback callback);
  void Unpause(void* context, EventHandledCallback callback);

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

  // deeplink
  void Deeplink(char* payload);

  void InjectInputEvent(SbInputData* data);

 protected:
  // --- Application overrides ---
  void Initialize() override;
  void Teardown() override;
  void OnSuspend() override;
  void OnResume() override;

  // --- QueueApplication overrides ---
  bool MayHaveSystemEvents() override;
  Event* PollNextSystemEvent() override;
  Event* WaitForSystemEventWithTimeout(SbTime time) override;
  void WakeSystemEventWait() override;

 protected:
  wl_display* display_;
  wl_compositor* compositor_;
  wl_shell* shell_;
  DevInput dev_input_;
  float video_pixel_ratio_;

 private:
  void InitializeEgl();
  void TerminateEgl();

  // wakeup event
  int wakeup_fd_;

  // observers
  Mutex observers_mutex_;
  std::deque<StateObserver*> observers_;
};

}  // namespace wayland
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WAYLAND_APPLICATION_WAYLAND_H_
