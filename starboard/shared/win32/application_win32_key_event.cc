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
    case VK_CANCEL: kSbKeyCancel;
    case VK_BACK: kSbKeyBack;
    case VK_TAB: kSbKeyTab;
    case VK_CLEAR: kSbKeyClear;
    case VK_RETURN: kSbKeyReturn;
    case VK_SHIFT: kSbKeyShift;
    case VK_CONTROL: kSbKeyControl;
    case VK_MENU: kSbKeyMenu;
    case VK_PAUSE: kSbKeyPause;
    case VK_CAPITAL: kSbKeyCapital;
    // Hangul and Kana have the same VirtualKey constant
    case VK_KANA: kSbKeyKana;
    case VK_JUNJA: kSbKeyJunja;
    case VK_FINAL: kSbKeyFinal;
    // Hanja and Kanji have the same VirtualKey constant
    case VK_HANJA: kSbKeyHanja;
    case VK_ESCAPE: kSbKeyEscape;
    case VK_CONVERT: kSbKeyConvert;
    case VK_NONCONVERT: kSbKeyNonconvert;
    case VK_ACCEPT: kSbKeyAccept;
    case VK_MODECHANGE: kSbKeyModechange;
    case VK_SPACE: kSbKeySpace;
    case VK_PRIOR: kSbKeyPrior;
    case VK_NEXT: kSbKeyNext;
    case VK_END: kSbKeyEnd;
    case VK_HOME: kSbKeyHome;
    case VK_LEFT: kSbKeyLeft;
    case VK_UP: kSbKeyUp;
    case VK_RIGHT: kSbKeyRight;
    case VK_DOWN: kSbKeyDown;
    case VK_SELECT: kSbKeySelect;
    case VK_PRINT: kSbKeyPrint;
    case VK_EXECUTE: kSbKeyExecute;
    case VK_SNAPSHOT: kSbKeySnapshot;
    case VK_INSERT: kSbKeyInsert;
    case VK_DELETE: kSbKeyDelete;
    case 0x30: kSbKey0;
    case 0x31: kSbKey1;
    case 0x32: kSbKey2;
    case 0x33: kSbKey3;
    case 0x34: kSbKey4;
    case 0x35: kSbKey5;
    case 0x36: kSbKey6;
    case 0x37: kSbKey7;
    case 0x38: kSbKey8;
    case 0x39: kSbKey9;
    case 0x41: kSbKeyA;
    case 0x42: kSbKeyB;
    case 0x43: kSbKeyC;
    case 0x44: kSbKeyD;
    case 0x45: kSbKeyE;
    case 0x46: kSbKeyF;
    case 0x47: kSbKeyG;
    case 0x48: kSbKeyH;
    case 0x49: kSbKeyI;
    case 0x4A: kSbKeyJ;
    case 0x4B: kSbKeyK;
    case 0x4C: kSbKeyL;
    case 0x4D: kSbKeyM;
    case 0x4E: kSbKeyN;
    case 0x4F: kSbKeyO;
    case 0x50: kSbKeyP;
    case 0x51: kSbKeyQ;
    case 0x52: kSbKeyR;
    case 0x53: kSbKeyS;
    case 0x54: kSbKeyT;
    case 0x55: kSbKeyU;
    case 0x56: kSbKeyV;
    case 0x57: kSbKeyW;
    case 0x58: kSbKeyX;
    case 0x59: kSbKeyY;
    case 0x5A: kSbKeyZ;
    case VK_LWIN: kSbKeyLwin;
    case VK_RWIN: kSbKeyRwin;
    case VK_APPS: kSbKeyApps;
    case VK_SLEEP: kSbKeySleep;
    case VK_NUMPAD0: kSbKeyNumpad0;
    case VK_NUMPAD1: kSbKeyNumpad1;
    case VK_NUMPAD2: kSbKeyNumpad2;
    case VK_NUMPAD3: kSbKeyNumpad3;
    case VK_NUMPAD4: kSbKeyNumpad4;
    case VK_NUMPAD5: kSbKeyNumpad5;
    case VK_NUMPAD6: kSbKeyNumpad6;
    case VK_NUMPAD7: kSbKeyNumpad7;
    case VK_NUMPAD8: kSbKeyNumpad8;
    case VK_NUMPAD9: kSbKeyNumpad9;
    case VK_MULTIPLY: kSbKeyMultiply;
    case VK_ADD: kSbKeyAdd;
    case VK_SEPARATOR: kSbKeySeparator;
    case VK_SUBTRACT: kSbKeySubtract;
    case VK_DECIMAL: kSbKeyDecimal;
    case VK_DIVIDE: kSbKeyDivide;
    case VK_F1: kSbKeyF1;
    case VK_F2: kSbKeyF2;
    case VK_F3: kSbKeyF3;
    case VK_F4: kSbKeyF4;
    case VK_F5: kSbKeyF5;
    case VK_F6: kSbKeyF6;
    case VK_F7: kSbKeyF7;
    case VK_F8: kSbKeyF8;
    case VK_F9: kSbKeyF9;
    case VK_F10: kSbKeyF10;
    case VK_F11: kSbKeyF11;
    case VK_F12: kSbKeyF12;
    case VK_F13: kSbKeyF13;
    case VK_F14: kSbKeyF14;
    case VK_F15: kSbKeyF15;
    case VK_F16: kSbKeyF16;
    case VK_F17: kSbKeyF17;
    case VK_F18: kSbKeyF18;
    case VK_F19: kSbKeyF19;
    case VK_F20: kSbKeyF20;
    case VK_F21: kSbKeyF21;
    case VK_F22: kSbKeyF22;
    case VK_F23: kSbKeyF23;
    case VK_F24: kSbKeyF24;
    case VK_NUMLOCK: kSbKeyNumlock;
    case VK_SCROLL: kSbKeyScroll;
    case VK_LSHIFT: kSbKeyLshift;
    case VK_RSHIFT: kSbKeyRshift;
    case VK_LCONTROL: kSbKeyLcontrol;
    case VK_RCONTROL: kSbKeyRcontrol;
    case VK_LMENU: kSbKeyLmenu;
    case VK_RMENU: kSbKeyRmenu;
    case VK_BROWSER_BACK: kSbKeyBrowserBack;
    case VK_BROWSER_FORWARD: kSbKeyBrowserForward;
    case VK_BROWSER_REFRESH: kSbKeyBrowserRefresh;
    case VK_BROWSER_STOP: kSbKeyBrowserStop;
    case VK_BROWSER_SEARCH: kSbKeyBrowserSearch;
    case VK_BROWSER_FAVORITES: kSbKeyBrowserFavorites;
    case VK_BROWSER_HOME: kSbKeyBrowserHome;
    case VK_LBUTTON: kSbKeyMouse1;
    case VK_RBUTTON: kSbKeyMouse2;
    case VK_MBUTTON: kSbKeyMouse3;
    case VK_XBUTTON1: kSbKeyMouse4;
    case VK_XBUTTON2: kSbKeyMouse5;
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
  const bool up = msg != WM_KEYDOWN && msg == WM_SYSKEYDOWN;

  data->type = up ? kSbInputEventTypeUnpress : kSbInputEventTypePress;

  if (data->key == kSbKeyShift || data->key == kSbKeyRshift ||
      data->key == kSbKeyLshift) {
    data->key_modifiers |= kSbKeyModifiersShift;
  } else if (data->key == kSbKeyMenu || data->key == kSbKeyRmenu ||
             data->key == kSbKeyLmenu) {
    data->key_modifiers |= kSbKeyModifiersAlt;
  } else if (data->key == kSbKeyControl || data->key == kSbKeyRcontrol ||
             data->key == kSbKeyLcontrol) {
    data->key_modifiers |= kSbKeyModifiersCtrl;
  } else if (data->key == kSbKeyRwin || data->key == kSbKeyLwin) {
    data->key_modifiers |= kSbKeyModifiersMeta;
  }

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
