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

#ifndef STARBOARD_SHARED_WAYLAND_DEV_INPUT_H_
#define STARBOARD_SHARED_WAYLAND_DEV_INPUT_H_

#include <EGL/egl.h>
#include <linux/input.h>

#include "starboard/input.h"
#include "starboard/key.h"
#include "starboard/log.h"
#include "starboard/shared/wayland/application_wayland.h"
#include "starboard/shared/wayland/window_internal.h"

namespace starboard {
namespace shared {
namespace wayland {

#define KEY_INFO_BUTTON 0xbc

// Converts an input_event code into an SbKey.
static SbKey KeyCodeToSbKey(uint16_t code) {
  switch (code) {
    case KEY_BACKSPACE:
      return kSbKeyBack;
    case KEY_DELETE:
      return kSbKeyDelete;
    case KEY_TAB:
      return kSbKeyTab;
    case KEY_LINEFEED:
    case KEY_ENTER:
    case KEY_KPENTER:
      return kSbKeyReturn;
    case KEY_CLEAR:
      return kSbKeyClear;
    case KEY_SPACE:
      return kSbKeySpace;
    case KEY_HOME:
      return kSbKeyHome;
    case KEY_END:
      return kSbKeyEnd;
    case KEY_PAGEUP:
      return kSbKeyPrior;
    case KEY_PAGEDOWN:
      return kSbKeyNext;
    case KEY_LEFT:
      return kSbKeyLeft;
    case KEY_RIGHT:
      return kSbKeyRight;
    case KEY_DOWN:
      return kSbKeyDown;
    case KEY_UP:
      return kSbKeyUp;
    case KEY_ESC:
      return kSbKeyEscape;
    case KEY_KATAKANA:
    case KEY_HIRAGANA:
    case KEY_KATAKANAHIRAGANA:
      return kSbKeyKana;
    case KEY_HANGEUL:
      return kSbKeyHangul;
    case KEY_HANJA:
      return kSbKeyHanja;
    case KEY_HENKAN:
      return kSbKeyConvert;
    case KEY_MUHENKAN:
      return kSbKeyNonconvert;
    case KEY_ZENKAKUHANKAKU:
      return kSbKeyDbeDbcschar;
    case KEY_A:
      return kSbKeyA;
    case KEY_B:
      return kSbKeyB;
    case KEY_C:
      return kSbKeyC;
    case KEY_D:
      return kSbKeyD;
    case KEY_E:
      return kSbKeyE;
    case KEY_F:
      return kSbKeyF;
    case KEY_G:
      return kSbKeyG;
    case KEY_H:
      return kSbKeyH;
    case KEY_I:
      return kSbKeyI;
    case KEY_J:
      return kSbKeyJ;
    case KEY_K:
      return kSbKeyK;
    case KEY_L:
      return kSbKeyL;
    case KEY_M:
      return kSbKeyM;
    case KEY_N:
      return kSbKeyN;
    case KEY_O:
      return kSbKeyO;
    case KEY_P:
      return kSbKeyP;
    case KEY_Q:
      return kSbKeyQ;
    case KEY_R:
      return kSbKeyR;
    case KEY_S:
      return kSbKeyS;
    case KEY_T:
      return kSbKeyT;
    case KEY_U:
      return kSbKeyU;
    case KEY_V:
      return kSbKeyV;
    case KEY_W:
      return kSbKeyW;
    case KEY_X:
      return kSbKeyX;
    case KEY_Y:
      return kSbKeyY;
    case KEY_Z:
      return kSbKeyZ;

    case KEY_0:
      return kSbKey0;
    case KEY_1:
      return kSbKey1;
    case KEY_2:
      return kSbKey2;
    case KEY_3:
      return kSbKey3;
    case KEY_4:
      return kSbKey4;
    case KEY_5:
      return kSbKey5;
    case KEY_6:
      return kSbKey6;
    case KEY_7:
      return kSbKey7;
    case KEY_8:
      return kSbKey8;
    case KEY_9:
      return kSbKey9;

    case KEY_NUMERIC_0:
    case KEY_NUMERIC_1:
    case KEY_NUMERIC_2:
    case KEY_NUMERIC_3:
    case KEY_NUMERIC_4:
    case KEY_NUMERIC_5:
    case KEY_NUMERIC_6:
    case KEY_NUMERIC_7:
    case KEY_NUMERIC_8:
    case KEY_NUMERIC_9:
      return static_cast<SbKey>(kSbKey0 + (code - KEY_NUMERIC_0));

    case KEY_KP0:
      return kSbKeyNumpad0;
    case KEY_KP1:
      return kSbKeyNumpad1;
    case KEY_KP2:
      return kSbKeyNumpad2;
    case KEY_KP3:
      return kSbKeyNumpad3;
    case KEY_KP4:
      return kSbKeyNumpad4;
    case KEY_KP5:
      return kSbKeyNumpad5;
    case KEY_KP6:
      return kSbKeyNumpad6;
    case KEY_KP7:
      return kSbKeyNumpad7;
    case KEY_KP8:
      return kSbKeyNumpad8;
    case KEY_KP9:
      return kSbKeyNumpad9;

    case KEY_KPASTERISK:
      return kSbKeyMultiply;
    case KEY_KPDOT:
      return kSbKeyDecimal;
    case KEY_KPSLASH:
      return kSbKeyDivide;
    case KEY_KPPLUS:
    case KEY_EQUAL:
      return kSbKeyOemPlus;
    case KEY_COMMA:
      return kSbKeyOemComma;
    case KEY_KPMINUS:
    case KEY_MINUS:
      return kSbKeyOemMinus;
    case KEY_DOT:
      return kSbKeyOemPeriod;
    case KEY_SEMICOLON:
      return kSbKeyOem1;
    case KEY_SLASH:
      return kSbKeyOem2;
    case KEY_GRAVE:
      return kSbKeyOem3;
    case KEY_LEFTBRACE:
      return kSbKeyOem4;
    case KEY_BACKSLASH:
      return kSbKeyOem5;
    case KEY_RIGHTBRACE:
      return kSbKeyOem6;
    case KEY_APOSTROPHE:
      return kSbKeyOem7;
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
      return kSbKeyShift;
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
      return kSbKeyControl;
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
      return kSbKeyMenu;
    case KEY_PAUSE:
      return kSbKeyPause;
    case KEY_CAPSLOCK:
      return kSbKeyCapital;
    case KEY_NUMLOCK:
      return kSbKeyNumlock;
    case KEY_SCROLLLOCK:
      return kSbKeyScroll;
    case KEY_SELECT:
      return kSbKeySelect;
    case KEY_PRINT:
      return kSbKeyPrint;
    case KEY_INSERT:
      return kSbKeyInsert;
    case KEY_HELP:
      return kSbKeyHelp;
    case KEY_MENU:
      return kSbKeyApps;
    case KEY_FN_F1:
    case KEY_FN_F2:
    case KEY_FN_F3:
    case KEY_FN_F4:
    case KEY_FN_F5:
    case KEY_FN_F6:
    case KEY_FN_F7:
    case KEY_FN_F8:
    case KEY_FN_F9:
    case KEY_FN_F10:
    case KEY_FN_F11:
    case KEY_FN_F12:
      return static_cast<SbKey>(kSbKeyF1 + (code - KEY_FN_F1));

    // For supporting multimedia buttons on a USB keyboard.
    case KEY_BACK:
      return kSbKeyBrowserBack;
    case KEY_FORWARD:
      return kSbKeyBrowserForward;
    case KEY_REFRESH:
      return kSbKeyBrowserRefresh;
    case KEY_STOP:
      return kSbKeyBrowserStop;
    case KEY_SEARCH:
      return kSbKeyBrowserSearch;
    case KEY_FAVORITES:
      return kSbKeyBrowserFavorites;
    case KEY_HOMEPAGE:
      return kSbKeyBrowserHome;
    case KEY_MUTE:
      return kSbKeyVolumeMute;
    case KEY_VOLUMEDOWN:
      return kSbKeyVolumeDown;
    case KEY_VOLUMEUP:
      return kSbKeyVolumeUp;
    case KEY_NEXTSONG:
      return kSbKeyMediaNextTrack;
    case KEY_PREVIOUSSONG:
      return kSbKeyMediaPrevTrack;
    case KEY_STOPCD:
      return kSbKeyMediaStop;
    case KEY_PLAYPAUSE:
      return kSbKeyMediaPlayPause;
    case KEY_MAIL:
      return kSbKeyMediaLaunchMail;
    case KEY_CALC:
      return kSbKeyMediaLaunchApp2;
    case KEY_WLAN:
      return kSbKeyWlan;
    case KEY_POWER:
      return kSbKeyPower;
    case KEY_BRIGHTNESSDOWN:
      return kSbKeyBrightnessDown;
    case KEY_BRIGHTNESSUP:
      return kSbKeyBrightnessUp;

    case KEY_INFO_BUTTON:
      return kSbKeyF1;
  }
  SB_DLOG(WARNING) << "Unknown code: 0x" << std::hex << code;
  return kSbKeyUnknown;
}  // NOLINT(readability/fn_size)

// Get a SbKeyLocation from an input_event.code.
static SbKeyLocation KeyCodeToSbKeyLocation(uint16_t code) {
  switch (code) {
    case KEY_LEFTALT:
    case KEY_LEFTCTRL:
    case KEY_LEFTMETA:
    case KEY_LEFTSHIFT:
      return kSbKeyLocationLeft;
    case KEY_RIGHTALT:
    case KEY_RIGHTCTRL:
    case KEY_RIGHTMETA:
    case KEY_RIGHTSHIFT:
      return kSbKeyLocationRight;
  }

  return kSbKeyLocationUnspecified;
}

// keyboard_listener
static void KeyboardHandleKeyMap(void* data,
                                 struct wl_keyboard* keyboard,
                                 uint32_t format,
                                 int fd,
                                 uint32_t size) {
  SB_DLOG(INFO) << "[Key] Keyboard keymap";
}

static void KeyboardHandleEnter(void* data,
                                struct wl_keyboard* keyboard,
                                uint32_t serial,
                                struct wl_surface* surface,
                                struct wl_array* keys) {
  SB_DLOG(INFO) << "[Key] Keyboard gained focus";
}

static void KeyboardHandleLeave(void* data,
                                struct wl_keyboard* keyboard,
                                uint32_t serial,
                                struct wl_surface* surface) {
  SB_DLOG(INFO) << "[Key] Keyboard lost focus";
  ApplicationWayland* wayland_window_ =
      reinterpret_cast<ApplicationWayland*> data;
  wayland_window_->DeleteRepeatKey();
}

static void KeyboardHandleKey(void* data,
                              struct wl_keyboard* keyboard,
                              uint32_t serial,
                              uint32_t time,
                              uint32_t key,
                              uint32_t state) {
  SB_DLOG(INFO) << "[Key] Key :" << key << ", state:" << state;
  ApplicationWayland* wayland_window_ =
      reinterpret_cast<ApplicationWayland*> data;
  if (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN) {
    wayland_window_->CreateKey(key, state, true);
  } else {
    wayland_window_->CreateKey(key, state, false);
  }
}

static void KeyboardHandleModifiers(void* data,
                                    struct wl_keyboard* keyboard,
                                    uint32_t serial,
                                    uint32_t mods_depressed,
                                    uint32_t mods_latched,
                                    uint32_t mods_locked,
                                    uint32_t group) {
  SB_DLOG(INFO) << "[Key] Modifiers depressed " << mods_depressed
                << ", latched " << mods_latched << ", locked " << mods_locked
                << ", group " << group;
}

static const struct wl_keyboard_listener keyboard_listener_ = {
    &KeyboardHandleKeyMap, &KeyboardHandleEnter,     &KeyboardHandleLeave,
    &KeyboardHandleKey,    &KeyboardHandleModifiers,
};

// seat_listener
static void SeatHandleCapabilities(void* data,
                                   struct wl_seat* seat,
                                   unsigned int caps) {
  ApplicationWayland* wayland_window_ =
      reinterpret_cast<ApplicationWayland*> data;
  if (!wayland_window_->GetKeyboard()) {
    SB_DLOG(INFO) << "[Key] seat_handle_capabilities caps: " << caps;
    if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
      SB_DLOG(INFO) << "[Key] wl_seat_get_keyboard";
      wayland_window_->SetKeyboard(wl_seat_get_keyboard(seat));
      wl_keyboard_add_listener(wayland_window_->GetKeyboard(),
                               &keyboard_listener, data);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD)) {
      SB_DLOG(INFO) << "[Key] wl_keyboard_destroy";
      wl_keyboard_destroy(wayland_window_->GetKeyboard());
      wayland_window_->SetKeyboard(NULL);
    }
  }
}

static const struct wl_seat_listener seat_listener = {
    &SeatHandleCapabilities,
};

// registry_listener
static void RegistryAddObject(void* data,
                              struct wl_registry* registry,
                              uint32_t name,
                              const char* interface,
                              uint32_t version) {
  ApplicationWayland* wayland_window_ =
      reinterpret_cast<ApplicationWayland*> data;
  if (strcmp(interface, "wl_compositor") == 0) {
    wayland_window_->SetCompositor(static_cast<wl_compositor*>(
        wl_registry_bind(registry, name, &wl_compositor_interface, 1)));
  } else if (strcmp(interface, "wl_shell") == 0) {
    wayland_window_->SetShell(static_cast<wl_shell*>(
        wl_registry_bind(registry, name, &wl_shell_interface, 1)));
  } else if (strcmp(interface, "wl_seat") == 0) {
    wayland_window_->SetSeat(static_cast<wl_seat*>(
        wl_registry_bind(registry, name, &wl_seat_interface, 1)));
    wl_seat_add_listener(wayland_window_->GetSeat(), &seat_listener, data);
  } else if (!strcmp(interface, "tizen_policy")) {
    wayland_window_->SetPolicy(static_cast<tizen_policy*>(
        wl_registry_bind(registry, name, &tizen_policy_interface, 1)));
  }
}

static void RegistryRemoveObject(void*, struct wl_registry*, uint32_t) {}

static struct wl_registry_listener registry_listener = {&RegistryAddObject,
                                                        &RegistryRemoveObject};

// shell_surface_listener
static void ShellSurfacePing(void*,
                             struct wl_shell_surface* shell_surface,
                             uint32_t serial) {
  wl_shell_surface_pong(shell_surface, serial);
}
static void ShellSurfaceConfigure(void* data,
                                  struct wl_shell_surface*,
                                  uint32_t,
                                  int32_t width,
                                  int32_t height) {
  SB_DLOG(INFO) << "shell_surface_configure width(" << width << "), height("
                << height << ")";
  if (width && height) {
    SbWindowPrivate* window = reinterpret_cast<SbWindowPrivate*> data;
    wl_egl_window_resize(window->egl_window, width, height, 0, 0);
  } else {
    SB_DLOG(INFO) << "width and height is 0. we don't resize that";
  }
}

static void ShellSurfacePopupDone(void*, struct wl_shell_surface*) {}

static struct wl_shell_surface_listener shell_surface_listener = {
    &ShellSurfacePing, &ShellSurfaceConfigure, &ShellSurfacePopupDone};

static void WindowCbVisibilityChange(void* data,
                                     struct tizen_visibility* tizen_visibility
                                         EINA_UNUSED,
                                     uint32_t visibility) {
  if (visibility == TIZEN_VISIBILITY_VISIBILITY_FULLY_OBSCURED)
    ApplicationWayland::Get()->Pause();
  else
    ApplicationWayland::Get()->Unpause();
}

static const struct tizen_visibility_listener tizen_visibility_listener = {
    WindowCbVisibilityChange};

}  // namespace wayland
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WAYLAND_DEV_INPUT_H_
