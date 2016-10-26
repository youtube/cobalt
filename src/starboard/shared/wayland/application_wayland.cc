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

#include "starboard/event.h"
#include "starboard/log.h"
#include "starboard/shared/wayland/window_internal.h"

namespace starboard {
namespace shared {
namespace wayland {

static struct wl_display* display;
static struct wl_compositor* compositor = NULL;
static struct wl_shell* shell = NULL;
static EGLDisplay egl_display;
static char running = 1;

// listeners
static void registry_add_object(void* data,
                                struct wl_registry* registry,
                                uint32_t name,
                                const char* interface,
                                uint32_t version) {
  if (!strcmp(interface, "wl_compositor")) {
    compositor = (wl_compositor*)wl_registry_bind(registry, name,
                                                  &wl_compositor_interface, 1);
  } else if (!strcmp(interface, "wl_shell")) {
    shell = (wl_shell*)wl_registry_bind(registry, name, &wl_shell_interface, 1);
  }
}
static void registry_remove_object(void* data,
                                   struct wl_registry* registry,
                                   uint32_t name) {}
static struct wl_registry_listener registry_listener = {
    &registry_add_object, &registry_remove_object};

static void shell_surface_ping(void* data,
                               struct wl_shell_surface* shell_surface,
                               uint32_t serial) {
  wl_shell_surface_pong(shell_surface, serial);
}
static void shell_surface_configure(void* data,
                                    struct wl_shell_surface* shell_surface,
                                    uint32_t edges,
                                    int32_t width,
                                    int32_t height) {
  SbWindowPrivate* window = (SbWindowPrivate*)data;
  wl_egl_window_resize(window->egl_window, width, height, 0, 0);
}
static void shell_surface_popup_done(void* data,
                                     struct wl_shell_surface* shell_surface) {}
static struct wl_shell_surface_listener shell_surface_listener = {
    &shell_surface_ping, &shell_surface_configure, &shell_surface_popup_done};

// Tizen application engine using the generic queue and a tizen implementation.
ApplicationWayland::ApplicationWayland() {}

ApplicationWayland::~ApplicationWayland() {}

SbWindow ApplicationWayland::CreateWindow(const SbWindowOptions* options) {
  SB_DLOG(INFO) << "CreateWindow";
  SbWindow window = new SbWindowPrivate(options);

  window->surface = wl_compositor_create_surface(compositor);
  window->shell_surface = wl_shell_get_shell_surface(shell, window->surface);
  wl_shell_surface_add_listener(window->shell_surface, &shell_surface_listener,
                                window);
  wl_shell_surface_set_toplevel(window->shell_surface);
  window->egl_window =
      wl_egl_window_create(window->surface, window->width, window->height);
  SB_DLOG(INFO) << "window: " << window << ", surface: " << window->surface
                << ", shell_surface: " << window->shell_surface
                << ", egl_window: " << window->egl_window;

  return window;
}

bool ApplicationWayland::DestroyWindow(SbWindow window) {
  SB_DLOG(INFO) << "DestroyWindow";
  if (!SbWindowIsValid(window)) {
    return false;
  }

  wl_egl_window_destroy(window->egl_window);
  wl_shell_surface_destroy(window->shell_surface);
  wl_surface_destroy(window->surface);
  return true;
}

void ApplicationWayland::Initialize() {
  SB_DLOG(INFO) << "Initialize";
  display = wl_display_connect(NULL);
  struct wl_registry* registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, NULL);
  wl_display_roundtrip(display);

  egl_display = eglGetDisplay(display);
  eglInitialize(egl_display, NULL, NULL);
  SB_DLOG(INFO) << "display: " << display << ", registry: " << registry
                << ", egl_display: " << egl_display;
}

void ApplicationWayland::Teardown() {
  SB_NOTIMPLEMENTED();
}

bool ApplicationWayland::MayHaveSystemEvents() {
  SB_NOTIMPLEMENTED();
  return false;
}

shared::starboard::Application::Event*
ApplicationWayland::PollNextSystemEvent() {
  SB_NOTIMPLEMENTED();
  return NULL;
}

shared::starboard::Application::Event*
ApplicationWayland::WaitForSystemEventWithTimeout(SbTime time) {
  SB_NOTIMPLEMENTED();
  return NULL;
}

void ApplicationWayland::WakeSystemEventWait() {
  SB_NOTIMPLEMENTED();
}

}  // namespace wayland
}  // namespace tizen
}  // namespace starboard
