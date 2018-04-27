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

#include "starboard/shared/wayland/application_wayland.h"

#include <EGL/egl.h>
#include <poll.h>
#include <sys/eventfd.h>

#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/wayland/dev_input.h"
#include "starboard/shared/wayland/window_internal.h"
#include "starboard/time.h"

// YouTube Technical Requirement 2018 (2016/11/1 - Initial draft)
// 9.5 The device MUST dispatch the following key events, as appropriate:
//  * Window.keydown
//      * After a key is held down for 500ms, the Window.keydown event
//        MUST repeat every 50ms until a user stops holding the key down.
//  * Window.keyup
static const SbTime kKeyHoldTime = 500 * kSbTimeMillisecond;
static const SbTime kKeyRepeatTime = 50 * kSbTimeMillisecond;

namespace starboard {
namespace shared {
namespace wayland {

// Tizen application engine using the generic queue and a tizen implementation.

ApplicationWayland::ApplicationWayland(float video_pixel_ratio)
    : video_pixel_ratio_(video_pixel_ratio),
      seat_(NULL),
      keyboard_(NULL),
      key_repeat_event_id_(kSbEventIdInvalid),
      key_repeat_interval_(kKeyHoldTime),
      key_modifiers_(0) {
  SbAudioSinkPrivate::Initialize();
}

SbWindow ApplicationWayland::CreateWindow(const SbWindowOptions* options) {
  SB_DLOG(INFO) << "CreateWindow";
  SbWindow window = new SbWindowPrivate(options, video_pixel_ratio_);
  window_ = window;

  // Graphics Plane
  window->surface = wl_compositor_create_surface(compositor_);
  window->shell_surface = wl_shell_get_shell_surface(shell_, window->surface);
  wl_shell_surface_add_listener(window->shell_surface, &shell_surface_listener,
                                window);

#if defined(COBALT_TIZEN)
  window->tz_visibility =
      tizen_policy_get_visibility(tz_policy_, window->surface);
  tizen_visibility_add_listener(window->tz_visibility,
                                &tizen_visibility_listener, window);
  tizen_policy_activate(tz_policy_, window->surface);
#endif
  wl_shell_surface_set_title(window->shell_surface, "cobalt");
  WindowRaise();

  struct wl_region* region;
  region = wl_compositor_create_region(compositor_);
  wl_region_add(region, 0, 0, window->width, window->height);
  wl_surface_set_opaque_region(window->surface, region);
  wl_region_destroy(region);

  window->egl_window =
      wl_egl_window_create(window->surface, window->width, window->height);

  return window;
}

bool ApplicationWayland::DestroyWindow(SbWindow window) {
  SB_DLOG(INFO) << "DestroyWindow";
  if (!SbWindowIsValid(window)) {
    SB_DLOG(WARNING) << "wayland window destroy failed!!";
    return false;
  }

  // Graphics Plane
#if defined(COBALT_TIZEN)
  tizen_visibility_destroy(window->tz_visibility);
#endif
  wl_egl_window_destroy(window->egl_window);
  wl_shell_surface_destroy(window->shell_surface);
  wl_surface_destroy(window->surface);

  return true;
}

void ApplicationWayland::Initialize() {
  SB_DLOG(INFO) << "Initialize";
  // Video Plane
  elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);

  // Graphics Plane
  display_ = wl_display_connect(NULL);
  struct wl_registry* registry = wl_display_get_registry(display_);
  wl_registry_add_listener(registry, &registry_listener, this);
  wl_display_dispatch(display_);
  wl_display_roundtrip(display_);

  // Open wakeup event
  wakeup_fd_ = eventfd(0, 0);
  if (wakeup_fd_ == -1)
    SB_DLOG(ERROR) << "wakeup_fd_ creation failed";

  InitializeEgl();
}

void ApplicationWayland::Teardown() {
  SB_DLOG(INFO) << "Teardown";
  DeleteRepeatKey();

  TerminateEgl();

  wl_display_flush(display_);
  wl_display_disconnect(display_);

  SbAudioSinkPrivate::TearDown();
  // Close wakeup event
  close(wakeup_fd_);
}

void ApplicationWayland::OnSuspend() {
  TerminateEgl();
}

void ApplicationWayland::OnResume() {
  InitializeEgl();
}

void ApplicationWayland::InitializeEgl() {
  EGLDisplay egl_display = eglGetDisplay(display_);
  SB_DLOG(INFO) << __FUNCTION__ << ": INITIALIZE egl_display=" << egl_display;
  SB_DCHECK(egl_display);
  eglInitialize(egl_display, NULL, NULL);
}

void ApplicationWayland::TerminateEgl() {
  EGLDisplay egl_display = eglGetDisplay(display_);
  SB_DLOG(INFO) << __FUNCTION__ << ": TERMINATE egl_display=" << egl_display;
  SB_DCHECK(egl_display);
  eglTerminate(egl_display);
}

bool ApplicationWayland::MayHaveSystemEvents() {
  // SB_DCHECK(SbThreadIsValid(wayland_thread_));
  return display_;
}

shared::starboard::Application::Event*
ApplicationWayland::PollNextSystemEvent() {
  // if queue is empty, try to read fd and push event to queue
  if (wl_display_prepare_read(display_) == 0) {
    wl_display_read_events(display_);
  }

  // dispatch queue if any event
  if (wl_display_dispatch_pending(display_) < 0) {
    SB_DLOG(ERROR) << "wl_display_dispatch_pending Error";
    return NULL;
  }

  return NULL;
}

shared::starboard::Application::Event*
ApplicationWayland::WaitForSystemEventWithTimeout(SbTime duration) {
  if (wl_display_flush(display_) < 0) {
    SB_DLOG(ERROR) << "wl_display_flush Error";
    return NULL;
  }

  struct pollfd fds[2];
  struct timespec timeout_ts;
  int ret;

  timeout_ts.tv_sec = duration / kSbTimeSecond;
  timeout_ts.tv_nsec =
      (duration % kSbTimeSecond) * kSbTimeNanosecondsPerMicrosecond;

  // wait wayland event
  fds[0].fd = wl_display_get_fd(display_);
  fds[0].events = POLLIN;
  fds[0].revents = 0;

  // wait wakeup event by event injection
  fds[1].fd = wakeup_fd_;
  fds[1].events = POLLIN;
  fds[1].revents = 0;

  ret = ppoll(fds, 2, &timeout_ts, NULL);

  if (timeout_ts.tv_sec > 0)  // long-wait log
    SB_DLOG(INFO) << "WaitForSystemEventWithTimeout : wakeup " << ret << " 0("
                  << fds[0].revents << ") 1(" << fds[1].revents << ")";

  if (ret > 0 && fds[1].revents & POLLIN) {  // clear wakeup event
    uint64_t u;
    read(wakeup_fd_, &u, sizeof(uint64_t));
  }

  // TODO : print log for too short waiting(under 2ms) to prevent abnormal fd
  // events.

  return NULL;
}

void ApplicationWayland::WakeSystemEventWait() {
  uint64_t u = 1;
  write(wakeup_fd_, &u, sizeof(uint64_t));
}

void ApplicationWayland::CreateRepeatKey() {
  if (!key_repeat_state_) {
    return;
  }

  if (key_repeat_interval_) {
    key_repeat_interval_ = kKeyRepeatTime;
  }

  CreateKey(key_repeat_key_, key_repeat_state_, true);
}

void ApplicationWayland::DeleteRepeatKey() {
  if (key_repeat_event_id_ != kSbEventIdInvalid) {
    SbEventCancel(key_repeat_event_id_);
    key_repeat_event_id_ = kSbEventIdInvalid;
  }
}

void ApplicationWayland::CreateKey(int key, int state, bool is_repeat) {
  SbInputData* data = new SbInputData();
  SbMemorySet(data, 0, sizeof(*data));
  data->window = window_;
  data->type = (state == 0 ? kSbInputEventTypeUnpress : kSbInputEventTypePress);
  data->device_type = kSbInputDeviceTypeRemote;
  data->device_id = 1;                           // kKeyboardDeviceId;
  data->key = KeyCodeToSbKey(key);
  data->key_location = KeyCodeToSbKeyLocation(key);
  data->key_modifiers = key_modifiers_;
  Inject(new Event(kSbEventTypeInput, data,
                   &Application::DeleteDestructor<SbInputData>));

  DeleteRepeatKey();

  if (is_repeat && state) {
    key_repeat_key_ = key;
    key_repeat_state_ = state;
    key_repeat_event_id_ = SbEventSchedule([](void* window) {
      ApplicationWayland* application =
          reinterpret_cast<ApplicationWayland*>(window);
      application->CreateRepeatKey();
    }, this, key_repeat_interval_);
  } else {
    key_repeat_interval_ = kKeyHoldTime;
  }
}

void ApplicationWayland::WindowRaise() {
#if defined(COBALT_TIZEN)
  if (tz_policy_)
    tizen_policy_raise(tz_policy_, window_->surface);
#endif
  if (window_->shell_surface)
    wl_shell_surface_set_toplevel(window_->shell_surface);
}

void ApplicationWayland::Pause(void* context, EventHandledCallback callback) {
  Application::Pause(context, callback);

  ScopedLock lock(observers_mutex_);
  std::for_each(observers_.begin(), observers_.end(),
                [](StateObserver* i) { i->OnAppPause(); });
}

void ApplicationWayland::Unpause(void* context, EventHandledCallback callback) {
  Application::Unpause(context, callback);

  ScopedLock lock(observers_mutex_);
  std::for_each(observers_.begin(), observers_.end(),
                [](StateObserver* i) { i->OnAppUnpause(); });
}

void ApplicationWayland::RegisterObserver(StateObserver* observer) {
  ScopedLock lock(observers_mutex_);
  observers_.push_back(observer);
}

void ApplicationWayland::UnregisterObserver(StateObserver* observer) {
  ScopedLock lock(observers_mutex_);
  auto it = std::find_if(
      observers_.begin(), observers_.end(),
      [observer](const StateObserver* i) { return observer == i; });
  if (it == observers_.end())
    return;
  observers_.erase(it);
}

void ApplicationWayland::Deeplink(char* payload) {
  const size_t payload_size = strlen(payload) + 1;
  char* copied_payload = new char[payload_size];
  snprintf(copied_payload, payload_size, "%s", payload);
  Inject(new Event(kSbEventTypeLink, copied_payload,
                   [](void* data) { delete[] reinterpret_cast<char*>(data); }));
}

}  // namespace wayland
}  // namespace shared
}  // namespace starboard
