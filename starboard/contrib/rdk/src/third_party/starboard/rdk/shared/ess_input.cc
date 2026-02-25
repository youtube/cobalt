//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
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

#include "third_party/starboard/rdk/shared/ess_input.h"

#include "starboard/common/log.h"
#include "starboard/event.h"
#include "starboard/input.h"
#include "starboard/key.h"

#include "third_party/starboard/rdk/shared/application_rdk.h"
#include "third_party/starboard/rdk/shared/linux_key_mapping.h"
#include "third_party/starboard/rdk/shared/log_override.h"
#include "third_party/starboard/rdk/shared/time_constants.h"

#include <linux/input.h>
#include <cstring>

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {

namespace {

// YouTube Technical Requirement 2018 (2016/11/1 - Initial draft)
// 9.5 The device MUST dispatch the following key events, as appropriate:
//  * Window.keydown
//      * After a key is held down for 500ms, the Window.keydown event
//        MUST repeat every 50ms until a user stops holding the key down.
//  * Window.keyup
constexpr int64_t kKeyHoldTime = 500 * kSbTimeMillisecond;
constexpr int64_t kKeyRepeatTime = 50 * kSbTimeMillisecond;

// Converts an input_event code into an SbKey.
SbKey KeyCodeToSbKey(uint16_t code) {
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

    case KEY_REWIND:
      return kSbKeyMediaRewind;
    case KEY_FASTFORWARD:
      return kSbKeyMediaFastForward;

    case KEY_RED:
      return kSbKeyRed;
    case KEY_GREEN:
      return kSbKeyGreen;
    case KEY_YELLOW:
      return kSbKeyYellow;
    case KEY_BLUE:
      return kSbKeyBlue;

    case KEY_CHANNELUP:
     return kSbKeyChannelUp;
    case KEY_CHANNELDOWN:
     return kSbKeyChannelDown;
    case KEY_INFO:
     return kSbKeyInfo;

    //[sky] mapping of the YouTube partner button
    case KEY_KPLEFTPAREN:
     return kSbKeyLaunchThisApplication;

    case KEY_F8:
     return kSbKeyMicrophone;

    case KEY_UNKNOWN:
      break;
  }
  SB_DLOG(WARNING) << "Unknown code: 0x" << std::hex << code;
  return kSbKeyUnknown;
}  // NOLINT(readability/fn_size)

// Get a SbKeyLocation from an input_event.code.
SbKeyLocation KeyCodeToSbKeyLocation(uint16_t code) {
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

SbKeyModifiers KeyCodeToSbKeyModifiers(uint16_t code) {
  switch (code) {
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
      return kSbKeyModifiersCtrl;
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
      return kSbKeyModifiersShift;
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
      return kSbKeyModifiersAlt;
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
      return kSbKeyModifiersMeta;
  }
  return kSbKeyModifiersNone;
}

}  // namespace

EssInput::EssInput() : key_repeat_interval_(kKeyHoldTime) {
}

EssInput::~EssInput() {
  DeleteRepeatKey();
}

void EssInput::CreateKey(unsigned int key, SbInputEventType type, unsigned int modifiers, bool repeatable) {
  SbKey sb_key = KeyCodeToSbKey(key);
  // if (sb_key == kSbKeyUnknown) {
  //   DeleteRepeatKey();
  //   return;
  // }

  SbInputData* data = new SbInputData();
  memset(data, 0, sizeof(*data));
  data->type = type;
  data->device_type = kSbInputDeviceTypeRemote;
  data->device_id = 1;  // kKeyboardDeviceId;
  data->key = sb_key;
  data->key_location = KeyCodeToSbKeyLocation(key);
  data->key_modifiers = modifiers;

  Application::Get()->InjectInputEvent(data);

  DeleteRepeatKey();

  if (repeatable && type == kSbInputEventTypePress) {
    key_repeat_key_ = key;
    key_repeat_state_ = 1;
    key_repeat_modifiers_ = modifiers;
    key_repeat_event_id_ = SbEventSchedule(
      [](void* data) {
        EssInput* ess_input = reinterpret_cast<EssInput*>(data);
        ess_input->CreateRepeatKey();
      },
      this, key_repeat_interval_);
  } else {
    key_repeat_interval_ = kKeyHoldTime;
  }
}

void EssInput::CreateRepeatKey() {
  if (!key_repeat_state_) {
    return;
  }
  if (key_repeat_interval_) {
    key_repeat_interval_ = kKeyRepeatTime;
  }
  CreateKey(key_repeat_key_, kSbInputEventTypePress, key_repeat_modifiers_, true);
}

void EssInput::DeleteRepeatKey() {
  key_repeat_state_ = 0;
  if (key_repeat_event_id_ != kSbEventIdInvalid) {
    SbEventCancel(key_repeat_event_id_);
    key_repeat_event_id_ = kSbEventIdInvalid;
  }
}

bool EssInput::UpdateModifiers(unsigned int key, SbInputEventType type) {
  SbKeyModifiers modifiers = KeyCodeToSbKeyModifiers(key);
  if (modifiers != kSbKeyModifiersNone) {
    if (type == kSbInputEventTypePress)
      key_modifiers_ |= modifiers;
    else
      key_modifiers_ &= ~modifiers;
    return true;
  }
  return false;
}

void EssInput::OnKeyboardKey(unsigned int key, SbInputEventType type) {
  if (UpdateModifiers(key, type))
    return;

  unsigned int modifiers = key_modifiers_;
  LinuxKeyMapping::MapKeyCodeAndModifiers(key, modifiers);

  bool repeatable =
    (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN ||
     key == KEY_FASTFORWARD || key == KEY_REWIND) && (modifiers == 0);

  if (type == kSbInputEventTypePress && repeatable && key == key_repeat_key_ && key_repeat_state_)
    return;

  static bool enable_key_debug = !!getenv("COBALT_ENABLE_KEY_DEBUG");
  if (enable_key_debug) {
    SB_LOG(INFO) << "OnKeyboardKey, key code = " << key
                 << ", type = " << type
                 << ", modifiers = " << modifiers
                 << ", repeatable = " << repeatable;
  }

  if (repeatable) {
    CreateKey(key, type, modifiers, true);
  } else {
    CreateKey(key, type, modifiers, false);
  }
}

void EssInput::OnKeyPressed(unsigned int key) {
  OnKeyboardKey(key, kSbInputEventTypePress);
}

void EssInput::OnKeyReleased(unsigned int key) {
  OnKeyboardKey(key, kSbInputEventTypeUnpress);
}

}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
