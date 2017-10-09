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

#include "starboard/shared/uwp/application_uwp.h"

#include "starboard/event.h"
#include "starboard/input.h"
#include "starboard/key.h"

using Windows::UI::Core::CoreWindow;
using Windows::UI::Core::KeyEventArgs;
using Windows::UI::Core::CoreVirtualKeyStates;
using Windows::System::VirtualKey;

namespace {

SbKey VirtualKeyToSbKey(VirtualKey key) {
// Disable warning for Invalid valid values from switch of enum.
#pragma warning(push)
#pragma warning(disable : 4063)
  switch (key) {
    case VirtualKey::None:
    case VirtualKey::NavigationView:
    case VirtualKey::NavigationMenu:
    case VirtualKey::NavigationUp:
    case VirtualKey::NavigationDown:
    case VirtualKey::NavigationLeft:
    case VirtualKey::NavigationRight:
    case VirtualKey::NavigationAccept:
    case VirtualKey::NavigationCancel:
      return kSbKeyUnknown;
    case VirtualKey::Cancel: return kSbKeyCancel;
    case VirtualKey::Back: return kSbKeyBack;
    case VirtualKey::Tab: return kSbKeyTab;
    case VirtualKey::Clear: return kSbKeyClear;
    case VirtualKey::Enter: return kSbKeyReturn;
    case VirtualKey::Shift: return kSbKeyShift;
    case VirtualKey::Control: return kSbKeyControl;
    case VirtualKey::Menu: return kSbKeyMenu;
    case VirtualKey::Pause: return kSbKeyPause;
    case VirtualKey::CapitalLock: return kSbKeyCapital;
    // Hangul and Kana have the same VirtualKey constant
    case VirtualKey::Kana: return kSbKeyKana;
    case VirtualKey::Junja: return kSbKeyJunja;
    case VirtualKey::Final: return kSbKeyFinal;
    // Hanja and Kanji have the same VirtualKey constant
    case VirtualKey::Hanja: return kSbKeyHanja;
    case VirtualKey::Escape: return kSbKeyEscape;
    case VirtualKey::Convert: return kSbKeyConvert;
    case VirtualKey::NonConvert: return kSbKeyNonconvert;
    case VirtualKey::Accept: return kSbKeyAccept;
    case VirtualKey::ModeChange: return kSbKeyModechange;
    case VirtualKey::Space: return kSbKeySpace;
    case VirtualKey::PageUp: return kSbKeyPrior;
    case VirtualKey::PageDown: return kSbKeyNext;
    case VirtualKey::End: return kSbKeyEnd;
    case VirtualKey::Home: return kSbKeyHome;
    case VirtualKey::Left: return kSbKeyLeft;
    case VirtualKey::Up: return kSbKeyUp;
    case VirtualKey::Right: return kSbKeyRight;
    case VirtualKey::Down: return kSbKeyDown;
    case VirtualKey::Select: return kSbKeySelect;
    case VirtualKey::Print: return kSbKeyPrint;
    case VirtualKey::Execute: return kSbKeyExecute;
    case VirtualKey::Snapshot: return kSbKeySnapshot;
    case VirtualKey::Insert: return kSbKeyInsert;
    case VirtualKey::Delete: return kSbKeyDelete;
    case VirtualKey::Number0: return kSbKey0;
    case VirtualKey::Number1: return kSbKey1;
    case VirtualKey::Number2: return kSbKey2;
    case VirtualKey::Number3: return kSbKey3;
    case VirtualKey::Number4: return kSbKey4;
    case VirtualKey::Number5: return kSbKey5;
    case VirtualKey::Number6: return kSbKey6;
    case VirtualKey::Number7: return kSbKey7;
    case VirtualKey::Number8: return kSbKey8;
    case VirtualKey::Number9: return kSbKey9;
    case VirtualKey::A: return kSbKeyA;
    case VirtualKey::B: return kSbKeyB;
    case VirtualKey::C: return kSbKeyC;
    case VirtualKey::D: return kSbKeyD;
    case VirtualKey::E: return kSbKeyE;
    case VirtualKey::F: return kSbKeyF;
    case VirtualKey::G: return kSbKeyG;
    case VirtualKey::H: return kSbKeyH;
    case VirtualKey::I: return kSbKeyI;
    case VirtualKey::J: return kSbKeyJ;
    case VirtualKey::K: return kSbKeyK;
    case VirtualKey::L: return kSbKeyL;
    case VirtualKey::M: return kSbKeyM;
    case VirtualKey::N: return kSbKeyN;
    case VirtualKey::O: return kSbKeyO;
    case VirtualKey::P: return kSbKeyP;
    case VirtualKey::Q: return kSbKeyQ;
    case VirtualKey::R: return kSbKeyR;
    case VirtualKey::S: return kSbKeyS;
    case VirtualKey::T: return kSbKeyT;
    case VirtualKey::U: return kSbKeyU;
    case VirtualKey::V: return kSbKeyV;
    case VirtualKey::W: return kSbKeyW;
    case VirtualKey::X: return kSbKeyX;
    case VirtualKey::Y: return kSbKeyY;
    case VirtualKey::Z: return kSbKeyZ;
    case VirtualKey::LeftWindows: return kSbKeyLwin;
    case VirtualKey::RightWindows: return kSbKeyRwin;
    case VirtualKey::Application: return kSbKeyApps;
    case VirtualKey::Sleep: return kSbKeySleep;
    case VirtualKey::NumberPad0: return kSbKeyNumpad0;
    case VirtualKey::NumberPad1: return kSbKeyNumpad1;
    case VirtualKey::NumberPad2: return kSbKeyNumpad2;
    case VirtualKey::NumberPad3: return kSbKeyNumpad3;
    case VirtualKey::NumberPad4: return kSbKeyNumpad4;
    case VirtualKey::NumberPad5: return kSbKeyNumpad5;
    case VirtualKey::NumberPad6: return kSbKeyNumpad6;
    case VirtualKey::NumberPad7: return kSbKeyNumpad7;
    case VirtualKey::NumberPad8: return kSbKeyNumpad8;
    case VirtualKey::NumberPad9: return kSbKeyNumpad9;
    case VirtualKey::Multiply: return kSbKeyMultiply;
    case VirtualKey::Add: return kSbKeyAdd;
    case VirtualKey::Separator: return kSbKeySeparator;
    case VirtualKey::Subtract: return kSbKeySubtract;
    case VirtualKey::Decimal: return kSbKeyDecimal;
    case VirtualKey::Divide: return kSbKeyDivide;
    case VirtualKey::F1: return kSbKeyF1;
    case VirtualKey::F2: return kSbKeyF2;
    case VirtualKey::F3: return kSbKeyF3;
    case VirtualKey::F4: return kSbKeyF4;
    case VirtualKey::F5: return kSbKeyF5;
    case VirtualKey::F6: return kSbKeyF6;
    case VirtualKey::F7: return kSbKeyF7;
    case VirtualKey::F8: return kSbKeyF8;
    case VirtualKey::F9: return kSbKeyF9;
    case VirtualKey::F10: return kSbKeyF10;
    case VirtualKey::F11: return kSbKeyF11;
    case VirtualKey::F12: return kSbKeyF12;
    case VirtualKey::F13: return kSbKeyF13;
    case VirtualKey::F14: return kSbKeyF14;
    case VirtualKey::F15: return kSbKeyF15;
    case VirtualKey::F16: return kSbKeyF16;
    case VirtualKey::F17: return kSbKeyF17;
    case VirtualKey::F18: return kSbKeyF18;
    case VirtualKey::F19: return kSbKeyF19;
    case VirtualKey::F20: return kSbKeyF20;
    case VirtualKey::F21: return kSbKeyF21;
    case VirtualKey::F22: return kSbKeyF22;
    case VirtualKey::F23: return kSbKeyF23;
    case VirtualKey::F24: return kSbKeyF24;
    // SbKeys were originally modeled after the windows virtual key mappings [1].
    // UWP VirtualKey uses a very similar mapping, but the UWP enum does not
    // contain all of the Virtual-Key Codes.
    // [1] https://msdn.microsoft.com/en-us/library/windows/desktop/dd375731%28v=vs.85%29.aspx
    case 0xBA: return kSbKeyOem1;  // Used for ";:" key with US keyboards.
    case 0xBB: return kSbKeyOemPlus;
    case 0xBC: return kSbKeyOemComma;
    case 0xBD: return kSbKeyOemMinus;
    case 0xBE: return kSbKeyOemPeriod;
    case 0xBF: return kSbKeyOem2;  // Used for "/?" key with US keyboards.
    case 0xC0: return kSbKeyOem3;  // Used for "~" with US keyboards.
    case 0xDB: return kSbKeyOem4;  // Used for "[{" with US keyboards.
    case 0xDC: return kSbKeyOem5;  // Used for "\|" with US keyboards.
    case 0xDD: return kSbKeyOem6;  // Used for "]}" with US keyboards.
    case 0xDE: return kSbKeyOem7;  // Used for quotes with US keyboards.
    case 0xDF: return kSbKeyOem8;  // Used for misc. chars with US keyboards.
    case 0xE2: return kSbKeyOem102;  // Used for "/" or angle bracket keys.
    case VirtualKey::NumberKeyLock: return kSbKeyNumlock;
    case VirtualKey::Scroll: return kSbKeyScroll;
    case VirtualKey::LeftShift: return kSbKeyLshift;
    case VirtualKey::RightShift: return kSbKeyRshift;
    case VirtualKey::LeftControl: return kSbKeyLcontrol;
    case VirtualKey::RightControl: return kSbKeyRcontrol;
    case VirtualKey::LeftMenu: return kSbKeyLmenu;
    case VirtualKey::RightMenu: return kSbKeyRmenu;
    case VirtualKey::GoBack: return kSbKeyBrowserBack;
    case VirtualKey::GoForward: return kSbKeyBrowserForward;
    case VirtualKey::Refresh: return kSbKeyBrowserRefresh;
    case VirtualKey::Stop: return kSbKeyBrowserStop;
    case VirtualKey::Search: return kSbKeyBrowserSearch;
    case VirtualKey::Favorites: return kSbKeyBrowserFavorites;
    case VirtualKey::GoHome: return kSbKeyBrowserHome;
    case VirtualKey::LeftButton: return kSbKeyMouse1;
    case VirtualKey::RightButton: return kSbKeyMouse2;
    case VirtualKey::MiddleButton: return kSbKeyMouse3;
    case VirtualKey::XButton1: return kSbKeyMouse4;
    case VirtualKey::XButton2: return kSbKeyMouse5;
    case VirtualKey::GamepadA: return kSbKeyGamepad1;
    case VirtualKey::GamepadB: return kSbKeyGamepad2;
    case VirtualKey::GamepadX: return kSbKeyGamepad3;
    case VirtualKey::GamepadY: return kSbKeyGamepad4;
    case VirtualKey::GamepadRightShoulder: return kSbKeyGamepadRightBumper;
    case VirtualKey::GamepadLeftShoulder: return kSbKeyGamepadLeftBumper;
    case VirtualKey::GamepadLeftTrigger: return kSbKeyGamepadLeftTrigger;
    case VirtualKey::GamepadRightTrigger: return kSbKeyGamepadRightTrigger;
    case VirtualKey::GamepadDPadUp: return kSbKeyGamepadDPadUp;
    case VirtualKey::GamepadDPadDown: return kSbKeyGamepadDPadDown;
    case VirtualKey::GamepadDPadLeft: return kSbKeyGamepadDPadLeft;
    case VirtualKey::GamepadDPadRight: return kSbKeyGamepadDPadRight;
    case VirtualKey::GamepadMenu: return kSbKeyGamepad6;
    case VirtualKey::GamepadView: return kSbKeyGamepad5;
    case VirtualKey::GamepadLeftThumbstickButton:
      return kSbKeyGamepadLeftStick;
    case VirtualKey::GamepadRightThumbstickButton:
      return kSbKeyGamepadRightStick;
    case VirtualKey::GamepadLeftThumbstickUp:
      return kSbKeyGamepadLeftStickUp;
    case VirtualKey::GamepadLeftThumbstickDown:
      return kSbKeyGamepadLeftStickDown;
    case VirtualKey::GamepadLeftThumbstickRight:
      return kSbKeyGamepadLeftStickRight;
    case VirtualKey::GamepadLeftThumbstickLeft:
      return kSbKeyGamepadLeftStickLeft;
    case VirtualKey::GamepadRightThumbstickUp:
      return kSbKeyGamepadRightStickUp;
    case VirtualKey::GamepadRightThumbstickDown:
      return kSbKeyGamepadRightStickDown;
    case VirtualKey::GamepadRightThumbstickRight:
      return kSbKeyGamepadRightStickRight;
    case VirtualKey::GamepadRightThumbstickLeft:
      return kSbKeyGamepadRightStickLeft;
    default:
      return kSbKeyUnknown;
  }
#pragma warning(pop)  // Warning 4093 (Invalid valid values from switch of enum)
}

// Returns true if a given VirtualKey is currently being held down.
bool IsDown(CoreWindow^ sender, VirtualKey key) {
  return ((sender->GetKeyState(key) & CoreVirtualKeyStates::Down) ==
          CoreVirtualKeyStates::Down);
}

}  // namespace

namespace starboard {
namespace shared {
namespace uwp {

void ApplicationUwp::OnKeyEvent(
    CoreWindow^ sender, KeyEventArgs^ args, bool up) {
  args->Handled = true;
  SbInputData* data = new SbInputData();
  SbMemorySet(data, 0, sizeof(*data));

  data->window = window_;
  data->device_type = kSbInputDeviceTypeKeyboard;
  data->device_id = device_id();
  data->key = VirtualKeyToSbKey(args->VirtualKey);

  if (up) {
    data->type = kSbInputEventTypeUnpress;
  } else {
    data->type = kSbInputEventTypePress;
  }

  // Build up key_modifiers
  if (IsDown(sender, VirtualKey::Menu) ||
      IsDown(sender, VirtualKey::LeftMenu) ||
      IsDown(sender, VirtualKey::RightMenu)) {
    data->key_modifiers |= kSbKeyModifiersAlt;
  }
  if (IsDown(sender, VirtualKey::Control) ||
      IsDown(sender, VirtualKey::LeftControl) ||
      IsDown(sender, VirtualKey::RightControl)) {
    data->key_modifiers |= kSbKeyModifiersCtrl;
  }
  if (IsDown(sender, VirtualKey::LeftWindows) ||
      IsDown(sender, VirtualKey::RightWindows)) {
    data->key_modifiers |= kSbKeyModifiersMeta;
  }
  if (IsDown(sender, VirtualKey::Shift) ||
      IsDown(sender, VirtualKey::LeftShift) ||
      IsDown(sender, VirtualKey::RightShift)) {
    data->key_modifiers |= kSbKeyModifiersShift;
  }

  // Set key_location
  switch (args->VirtualKey) {
    case VirtualKey::LeftMenu:
    case VirtualKey::LeftControl:
    case VirtualKey::LeftWindows:
    case VirtualKey::LeftShift:
      data->key_location = kSbKeyLocationLeft;
      break;
    case VirtualKey::RightMenu:
    case VirtualKey::RightControl:
    case VirtualKey::RightWindows:
    case VirtualKey::RightShift:
      data->key_location = kSbKeyLocationRight;
      break;
    default:
      break;
  }

  DispatchAndDelete(new Event(kSbEventTypeInput, data,
      &Application::DeleteDestructor<SbInputData>));
}

}  // namespace uwp
}  // namespace shared
}  // namespace starboard
