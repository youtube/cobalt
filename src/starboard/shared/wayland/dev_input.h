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

#ifndef STARBOARD_SHARED_WAYLAND_DEV_INPUT_H_
#define STARBOARD_SHARED_WAYLAND_DEV_INPUT_H_

#include <wayland-client.h>

#include "starboard/event.h"
#include "starboard/time.h"
#include "starboard/window.h"

namespace starboard {
namespace shared {
namespace wayland {

class DevInput {
 public:
  DevInput();
  ~DevInput() {}

  // wl registry add listener
  bool OnGlobalObjectAvailable(struct wl_registry* registry,
                               uint32_t name,
                               const char* interface,
                               uint32_t version);

  // wl seat add listener
  void OnSeatCapabilitiesChanged(struct wl_seat* seat, unsigned int caps);

  // wl keyboard add listener
  void OnKeyboardHandleKeyMap(struct wl_keyboard* keyboard,
                              uint32_t format,
                              int fd,
                              uint32_t size);
  void OnKeyboardHandleEnter(struct wl_keyboard* keyboard,
                             uint32_t serial,
                             struct wl_surface* surface,
                             struct wl_array* keys);
  void OnKeyboardHandleLeave(struct wl_keyboard* keyboard,
                             uint32_t serial,
                             struct wl_surface* surface);
  void OnKeyboardHandleKey(struct wl_keyboard* keyboard,
                           uint32_t serial,
                           uint32_t time,
                           uint32_t key,
                           uint32_t state);
  void OnKeyboardHandleModifiers(struct wl_keyboard* keyboard,
                                 uint32_t serial,
                                 uint32_t mods_depressed,
                                 uint32_t mods_latched,
                                 uint32_t mods_locked,
                                 uint32_t group);

  // window
  void SetSbWindow(SbWindow window) { window_ = window; }

  void DeleteRepeatKey();
  void CreateKey(int key, int state, bool is_repeat);
  void CreateRepeatKey();

 private:
  wl_seat* wl_seat_;
  wl_keyboard* wl_keyboard_;
  int key_repeat_key_;
  int key_repeat_state_;
  SbEventId key_repeat_event_id_;
  SbTime key_repeat_interval_;
  unsigned int key_modifiers_;
  SbWindow window_;
};

}  // namespace wayland
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WAYLAND_DEV_INPUT_H_
