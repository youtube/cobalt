// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "starboard/common/log.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/wayland/window_internal.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace wayland {

namespace {

// registry_listener
void GlobalObjectAvailable(void* data,
                           struct wl_registry* registry,
                           uint32_t name,
                           const char* interface,
                           uint32_t version) {
  ApplicationWayland* app_wl = reinterpret_cast<ApplicationWayland*>(data);
  SB_DCHECK(app_wl);
  app_wl->OnGlobalObjectAvailable(registry, name, interface, version);
}

void GlobalObjectRemove(void*, struct wl_registry*, uint32_t) {}

static struct wl_registry_listener registry_listener = {&GlobalObjectAvailable,
                                                        &GlobalObjectRemove};

}  // namespace

// Tizen application engine using the generic queue and a tizen implementation.
ApplicationWayland::ApplicationWayland(float video_pixel_ratio)
    : video_pixel_ratio_(video_pixel_ratio) {
  SbAudioSinkPrivate::Initialize();
}

void ApplicationWayland::InjectInputEvent(SbInputData* data) {
  Inject(new Event(kSbEventTypeInput, data,
                   &Application::DeleteDestructor<SbInputData>));
}

bool ApplicationWayland::OnGlobalObjectAvailable(struct wl_registry* registry,
                                                 uint32_t name,
                                                 const char* interface,
                                                 uint32_t version) {
  if (strcmp(interface, "wl_compositor") == 0) {
    compositor_ = static_cast<wl_compositor*>(
        wl_registry_bind(registry, name, &wl_compositor_interface, 1));
    return true;
  }
  if (strcmp(interface, "wl_shell") == 0) {
    shell_ = static_cast<wl_shell*>(
        wl_registry_bind(registry, name, &wl_shell_interface, 1));
    return true;
  }
  return dev_input_.OnGlobalObjectAvailable(registry, name, interface, version);
}

SbWindow ApplicationWayland::CreateWindow(const SbWindowOptions* options) {
  SB_DLOG(INFO) << "CreateWindow";
  SbWindow window =
      new SbWindowPrivate(compositor_, shell_, options, video_pixel_ratio_);
  dev_input_.SetSbWindow(window);
  return window;
}

bool ApplicationWayland::DestroyWindow(SbWindow window) {
  SB_DLOG(INFO) << "DestroyWindow";
  if (!SbWindowIsValid(window)) {
    SB_DLOG(WARNING) << "wayland window destroy failed!!";
    return false;
  }
  dev_input_.SetSbWindow(kSbWindowInvalid);
  delete window;
  return true;
}

void ApplicationWayland::Initialize() {
  SB_DLOG(INFO) << "Initialize";

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
  dev_input_.DeleteRepeatKey();

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
