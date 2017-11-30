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

#include "starboard/shared/win32/application_win32.h"

#include <windows.h>

#include "starboard/event.h"
#include "starboard/input.h"
#include "starboard/key.h"
#include "starboard/shared/starboard/application.h"

using starboard::shared::starboard::Application;

namespace {

const int kSbKeyboardDeviceId = 1;

SbKey VirtualKeyCodeToSbKey(WPARAM virtual_key_code) {
  // Keyboard code reference:
  // https://msdn.microsoft.com/en-us/library/windows/desktop/dd375731(v=vs.85).aspx
  switch (virtual_key_code) {
    case VK_CANCEL: return kSbKeyCancel;
    case VK_BACK: return kSbKeyBack;
    case VK_TAB: return kSbKeyTab;
    case VK_CLEAR: return kSbKeyClear;
    case VK_RETURN: return kSbKeyReturn;
    case VK_SHIFT: return kSbKeyShift;
    case VK_CONTROL: return kSbKeyControl;
    case VK_MENU: return kSbKeyMenu;
    case VK_PAUSE: return kSbKeyPause;
    case VK_CAPITAL: return kSbKeyCapital;
    // Hangul and Kana have the same VirtualKey constant
    case VK_KANA: return kSbKeyKana;
    case VK_JUNJA: return kSbKeyJunja;
    case VK_FINAL: return kSbKeyFinal;
    // Hanja and Kanji have the same VirtualKey constant
    case VK_HANJA: return kSbKeyHanja;
    case VK_ESCAPE: return kSbKeyEscape;
    case VK_CONVERT: return kSbKeyConvert;
    case VK_NONCONVERT: return kSbKeyNonconvert;
    case VK_ACCEPT: return kSbKeyAccept;
    case VK_MODECHANGE: return kSbKeyModechange;
    case VK_SPACE: return kSbKeySpace;
    case VK_PRIOR: return kSbKeyPrior;
    case VK_NEXT: return kSbKeyNext;
    case VK_END: return kSbKeyEnd;
    case VK_HOME: return kSbKeyHome;
    case VK_LEFT: return kSbKeyLeft;
    case VK_UP: return kSbKeyUp;
    case VK_RIGHT: return kSbKeyRight;
    case VK_DOWN: return kSbKeyDown;
    case VK_SELECT: return kSbKeySelect;
    case VK_PRINT: return kSbKeyPrint;
    case VK_EXECUTE: return kSbKeyExecute;
    case VK_SNAPSHOT: return kSbKeySnapshot;
    case VK_INSERT: return kSbKeyInsert;
    case VK_DELETE: return kSbKeyDelete;
    case VK_OEM_PERIOD: return kSbKeyOemPeriod;
    case 0x30: return kSbKey0;
    case 0x31: return kSbKey1;
    case 0x32: return kSbKey2;
    case 0x33: return kSbKey3;
    case 0x34: return kSbKey4;
    case 0x35: return kSbKey5;
    case 0x36: return kSbKey6;
    case 0x37: return kSbKey7;
    case 0x38: return kSbKey8;
    case 0x39: return kSbKey9;
    case 0x41: return kSbKeyA;
    case 0x42: return kSbKeyB;
    case 0x43: return kSbKeyC;
    case 0x44: return kSbKeyD;
    case 0x45: return kSbKeyE;
    case 0x46: return kSbKeyF;
    case 0x47: return kSbKeyG;
    case 0x48: return kSbKeyH;
    case 0x49: return kSbKeyI;
    case 0x4A: return kSbKeyJ;
    case 0x4B: return kSbKeyK;
    case 0x4C: return kSbKeyL;
    case 0x4D: return kSbKeyM;
    case 0x4E: return kSbKeyN;
    case 0x4F: return kSbKeyO;
    case 0x50: return kSbKeyP;
    case 0x51: return kSbKeyQ;
    case 0x52: return kSbKeyR;
    case 0x53: return kSbKeyS;
    case 0x54: return kSbKeyT;
    case 0x55: return kSbKeyU;
    case 0x56: return kSbKeyV;
    case 0x57: return kSbKeyW;
    case 0x58: return kSbKeyX;
    case 0x59: return kSbKeyY;
    case 0x5A: return kSbKeyZ;
    case VK_LWIN: return kSbKeyLwin;
    case VK_RWIN: return kSbKeyRwin;
    case VK_APPS: return kSbKeyApps;
    case VK_SLEEP: return kSbKeySleep;
    case VK_NUMPAD0: return kSbKeyNumpad0;
    case VK_NUMPAD1: return kSbKeyNumpad1;
    case VK_NUMPAD2: return kSbKeyNumpad2;
    case VK_NUMPAD3: return kSbKeyNumpad3;
    case VK_NUMPAD4: return kSbKeyNumpad4;
    case VK_NUMPAD5: return kSbKeyNumpad5;
    case VK_NUMPAD6: return kSbKeyNumpad6;
    case VK_NUMPAD7: return kSbKeyNumpad7;
    case VK_NUMPAD8: return kSbKeyNumpad8;
    case VK_NUMPAD9: return kSbKeyNumpad9;
    case VK_MULTIPLY: return kSbKeyMultiply;
    case VK_ADD: return kSbKeyAdd;
    case VK_SEPARATOR: return kSbKeySeparator;
    case VK_SUBTRACT: return kSbKeySubtract;
    case VK_DECIMAL: return kSbKeyDecimal;
    case VK_DIVIDE: return kSbKeyDivide;
    case VK_F1: return kSbKeyF1;
    case VK_F2: return kSbKeyF2;
    case VK_F3: return kSbKeyF3;
    case VK_F4: return kSbKeyF4;
    case VK_F5: return kSbKeyF5;
    case VK_F6: return kSbKeyF6;
    case VK_F7: return kSbKeyF7;
    case VK_F8: return kSbKeyF8;
    case VK_F9: return kSbKeyF9;
    case VK_F10: return kSbKeyF10;
    case VK_F11: return kSbKeyF11;
    case VK_F12: return kSbKeyF12;
    case VK_F13: return kSbKeyF13;
    case VK_F14: return kSbKeyF14;
    case VK_F15: return kSbKeyF15;
    case VK_F16: return kSbKeyF16;
    case VK_F17: return kSbKeyF17;
    case VK_F18: return kSbKeyF18;
    case VK_F19: return kSbKeyF19;
    case VK_F20: return kSbKeyF20;
    case VK_F21: return kSbKeyF21;
    case VK_F22: return kSbKeyF22;
    case VK_F23: return kSbKeyF23;
    case VK_F24: return kSbKeyF24;
    case VK_NUMLOCK: return kSbKeyNumlock;
    case VK_SCROLL: return kSbKeyScroll;
    case VK_LSHIFT: return kSbKeyLshift;
    case VK_RSHIFT: return kSbKeyRshift;
    case VK_LCONTROL: return kSbKeyLcontrol;
    case VK_RCONTROL: return kSbKeyRcontrol;
    case VK_LMENU: return kSbKeyLmenu;
    case VK_RMENU: return kSbKeyRmenu;
    case VK_BROWSER_BACK: return kSbKeyBrowserBack;
    case VK_BROWSER_FORWARD: return kSbKeyBrowserForward;
    case VK_BROWSER_REFRESH: return kSbKeyBrowserRefresh;
    case VK_BROWSER_STOP: return kSbKeyBrowserStop;
    case VK_BROWSER_SEARCH: return kSbKeyBrowserSearch;
    case VK_BROWSER_FAVORITES: return kSbKeyBrowserFavorites;
    case VK_BROWSER_HOME: return kSbKeyBrowserHome;
    case VK_LBUTTON: return kSbKeyMouse1;
    case VK_RBUTTON: return kSbKeyMouse2;
    case VK_MBUTTON: return kSbKeyMouse3;
    case VK_XBUTTON1: return kSbKeyMouse4;
    case VK_XBUTTON2: return kSbKeyMouse5;
    default:
      SB_LOG(WARNING) << "Unrecognized key hit.";
      return kSbKeyUnknown;
  }
}

}  // namespace

namespace starboard {
namespace shared {
namespace win32 {

// TODO: Plug into XInput APIs for Xbox controller input?
Application::Event* ApplicationWin32::ProcessWinKeyEvent(SbWindow window,
                                                         UINT msg,
                                                         WPARAM w_param,
                                                         LPARAM l_param) {
  SbInputData* data = new SbInputData();
  SbMemorySet(data, 0, sizeof(*data));

  data->window = window;
  data->device_type = kSbInputDeviceTypeKeyboard;
  // TODO: Do some more intelligent handling logic here to determine
  // a unique device ID.
  data->device_id = kSbKeyboardDeviceId;
  data->key = VirtualKeyCodeToSbKey(w_param);

  const bool was_down = ((l_param & (1 << 30)) != 0);
  const bool up = msg != WM_KEYDOWN && msg != WM_SYSKEYDOWN;

  data->type = up ? kSbInputEventTypeUnpress : kSbInputEventTypePress;

  SbKeyModifiers current_modifier = kSbKeyModifiersNone;
  if (data->key == kSbKeyShift || data->key == kSbKeyRshift ||
      data->key == kSbKeyLshift) {
    current_modifier = kSbKeyModifiersShift;
  } else if (data->key == kSbKeyMenu || data->key == kSbKeyRmenu ||
             data->key == kSbKeyLmenu) {
    current_modifier = kSbKeyModifiersAlt;
  } else if (data->key == kSbKeyControl || data->key == kSbKeyRcontrol ||
             data->key == kSbKeyLcontrol) {
    current_modifier = kSbKeyModifiersCtrl;
  } else if (data->key == kSbKeyRwin || data->key == kSbKeyLwin) {
    current_modifier = kSbKeyModifiersMeta;
  }

  // Either add or remove the current modifier key being pressed or released.
  // This noops for kSbKeyModifiersNone.
  if (up) {
    current_key_modifiers_ &= ~current_modifier;
  } else {
    current_key_modifiers_ |= current_modifier;
  }
  data->key_modifiers = current_key_modifiers_;

  switch (data->key) {
    case kSbKeyLshift:
    case kSbKeyLmenu:
    case kSbKeyLcontrol:
    case kSbKeyLwin:
      data->key_location = kSbKeyLocationLeft;
      break;
    case kSbKeyRshift:
    case kSbKeyRmenu:
    case kSbKeyRcontrol:
    case kSbKeyRwin:
      data->key_location = kSbKeyLocationRight;
      break;
    default:
      break;
  }

  return new Application::Event(kSbEventTypeInput, data,
                                &Application::DeleteDestructor<SbInputData>);
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
