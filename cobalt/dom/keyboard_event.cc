/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/keyboard_event.h"

#include "base/logging.h"
#include "cobalt/dom/event_names.h"
#include "cobalt/dom/keyboard_code.h"

namespace cobalt {
namespace dom {

KeyboardEvent::KeyboardEvent(const std::string& type, KeyLocationCode location,
                             unsigned int modifiers, int key_code,
                             int char_code, bool is_repeat)
    : UIEventWithKeyState(type, kBubbles, kCancelable, modifiers),
      location_(location),
      key_code_(key_code),
      char_code_(char_code),
      repeat_(is_repeat) {}

// How to determine keycode:
//   http://www.w3.org/TR/DOM-Level-3-Events/#determine-keydown-keyup-keyCode
// Virtual key code for keyup/keydown, 0 for keypress (split model)
int KeyboardEvent::key_code() const {
  if (type() == EventNames::GetInstance()->keydown() ||
      type() == EventNames::GetInstance()->keyup()) {
    return key_code_;
  }

  return 0;
}

int KeyboardEvent::char_code() const {
  return type() == EventNames::GetInstance()->keypress() ? char_code_ : 0;
}

std::string KeyboardEvent::key() const {
  // First check if the event corresponds to a printable character.
  // If so, just return a string containing that single character.
  int char_code = ComputeCharCode();
  if (char_code > 0 && char_code <= 127) {
    return std::string(1, static_cast<char>(char_code));
  }

  // Otherwise, we have one of the non-printable characters.
  // Definitions taken from:
  //   http://www.w3.org/TR/DOM-Level-3-Events-key/
  //   https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/key.
  switch (key_code_) {
    case kBack:
      return "Backspace";
    case kTab:
      return "Tab";
    case kBacktab:
      return "Tab";
    case kClear:
      return "Clear";
    case kReturn:
      return "Enter";
    case kShift:
      return "Shift";
    case kControl:
      return "Control";
    case kMenu:
      return "Alt";
    case kPause:
      return "Pause";
    case kCapital:
      return "CapsLock";
    case kKana:
      return "KanaMode";
    case kJunja:
      return "JunjaMode";
    case kFinal:
      return "HanjaMode";
    case kKanji:
      return "KanjiMode";
    case kEscape:
      return "Esc";
    case kConvert:
      return "Convert";
    case kNonconvert:
      return "Nonconvert";
    case kAccept:
      return "Accept";
    case kModechange:
      return "ModeChange";
    case kSpace:
      return " ";
    case kPrior:
      return "PageUp";
    case kNext:
      return "PageDown";
    case kEnd:
      return "End";
    case kHome:
      return "Home";
    case kLeft:
      return "ArrowLeft";
    case kUp:
      return "ArrowUp";
    case kRight:
      return "ArrowRight";
    case kDown:
      return "ArrowDown";
    case kSelect:
      return "Select";
    case kPrint:
      return "Print";
    case kExecute:
      return "Execute";
    case kSnapshot:
      return "PrintScreen";
    case kInsert:
      return "Insert";
    case kDelete:
      return "Delete";
    case kHelp:
      return "Help";
    case kLwin:
      return "Win";
    case kRwin:
      return "Win";
    case kApps:
      return "Apps";
    case kSleep:
      return "Standby";
    case kNumpad0:
      return "0";
    case kNumpad1:
      return "1";
    case kNumpad2:
      return "2";
    case kNumpad3:
      return "3";
    case kNumpad4:
      return "4";
    case kNumpad5:
      return "5";
    case kNumpad6:
      return "6";
    case kNumpad7:
      return "7";
    case kNumpad8:
      return "8";
    case kNumpad9:
      return "9";
    case kMultiply:
      return "Multiply";
    case kAdd:
      return "Add";
    case kSeparator:
      return "Separator";
    case kSubtract:
      return "Subtract";
    case kDecimal:
      return "Decimal";
    case kDivide:
      return "Divide";
    case kF1:
      return "F1";
    case kF2:
      return "F2";
    case kF3:
      return "F3";
    case kF4:
      return "F4";
    case kF5:
      return "F5";
    case kF6:
      return "F6";
    case kF7:
      return "F7";
    case kF8:
      return "F8";
    case kF9:
      return "F9";
    case kF10:
      return "F10";
    case kF11:
      return "F11";
    case kF12:
      return "F12";
    case kF13:
      return "F13";
    case kF14:
      return "F14";
    case kF15:
      return "F15";
    case kF16:
      return "F16";
    case kF17:
      return "F17";
    case kF18:
      return "F18";
    case kF19:
      return "F19";
    case kF20:
      return "F20";
    case kF21:
      return "F21";
    case kF22:
      return "F22";
    case kF23:
      return "F23";
    case kF24:
      return "F24";
    case kNumlock:
      return "NumLock";
    case kScroll:
      return "Scroll";
    case kWlan:
      return "Unidentified";
    case kPower:
      return "Unidentified";
    case kLshift:
    case kRshift:
      return "Shift";
    case kLcontrol:
    case kRcontrol:
      return "Control";
    case kLmenu:
    case kRmenu:
      return "Alt";
    case kBrowserBack:
      return "BrowserBack";
    case kBrowserForward:
      return "BrowserForward";
    case kBrowserRefresh:
      return "BrowserRefresh";
    case kBrowserStop:
      return "BrowserStop";
    case kBrowserSearch:
      return "BrowserSearch";
    case kBrowserFavorites:
      return "BrowserFavorites";
    case kBrowserHome:
      return "BrowserHome";
    case kVolumeMute:
      return "VolumeMute";
    case kVolumeDown:
      return "VolumeMute";
    case kVolumeUp:
      return "VolumeMute";
    case kMediaNextTrack:
      return "MediaNextTrack";
    case kMediaPrevTrack:
      return "MediaPrevTrack";
    case kMediaStop:
      return "MediaStop";
    case kMediaPlayPause:
      return "MediaPlayPause";
    case kMediaLaunchMail:
      return "LaunchMail";
    case kMediaLaunchMediaSelect:
      return "SelectMedia";
    case kMediaLaunchApp1:
      return "LaunchApplication1";
    case kMediaLaunchApp2:
      return "LaunchApplication1";
    case kBrightnessDown:
      return "BrightnessDown";
    case kBrightnessUp:
      return "BrightnessUp";
    case kPlay:
      return "Play";
    default:
      return "Unidentified";
  }
}

int KeyboardEvent::ComputeCharCode() const {
  if (shift_key()) {
    return KeyCodeToCharCodeWithShift(key_code_);
  } else {
    return KeyCodeToCharCodeNoShift(key_code_);
  }
}

// Static.
int KeyboardEvent::KeyCodeToCharCodeWithShift(int key_code) {
  // Characters are unaffected (keycode is uppercase by default).
  // Numbers map to the corresponding symbol.
  // Special symbols take on their shifted value.
  if (key_code >= kA && key_code <= kZ) {
    return key_code;
  }

  switch (key_code) {
    case k0:
      return ')';
    case k1:
      return '!';
    case k2:
      return '@';
    case k3:
      return '#';
    case k4:
      return '$';
    case k5:
      return '%';
    case k6:
      return '^';
    case k7:
      return '&';
    case k8:
      return '*';
    case k9:
      return '(';
    case kOem1:
      return ':';
    case kOemPlus:
      return '+';
    case kOemComma:
      return '<';
    case kOemMinus:
      return '_';
    case kOemPeriod:
      return '>';
    case kOem2:
      return '?';
    case kOem3:
      return '~';
    case kOem4:
      return '{';
    case kOem5:
      return '|';
    case kOem6:
      return '}';
    case kOem7:
      return '"';
    default:
      return 0;
  }
}

// Static.
int KeyboardEvent::KeyCodeToCharCodeNoShift(int key_code) {
  // Numbers are unaffected (keycode corresponds to correct Unicode value).
  // Characters are mapped from uppercase to lowercase.
  // Special symbols use their unshifted value.
  if (key_code >= k0 && key_code <= k9) {
    return key_code;
  }

  if (key_code >= kA && key_code <= kZ) {
    return key_code + 32;
  }

  switch (key_code) {
    case kOem1:
      return ';';
    case kOemPlus:
      return '=';
    case kOemComma:
      return ',';
    case kOemMinus:
      return '-';
    case kOemPeriod:
      return '.';
    case kOem2:
      return '/';
    case kOem3:
      return '`';
    case kOem4:
      return '[';
    case kOem5:
      return '\\';
    case kOem6:
      return ']';
    case kOem7:
      return '\'';
    default:
      return 0;
  }
}

// Static.
KeyboardEvent::KeyLocationCode KeyboardEvent::KeyCodeToKeyLocation(
    int key_code) {
  switch (key_code) {
    case kLshift:
    case kLcontrol:
    case kLmenu:
      return kDomKeyLocationLeft;
    case kRshift:
    case kRcontrol:
    case kRmenu:
      return kDomKeyLocationRight;
    default:
      return kDomKeyLocationStandard;
  }
}

}  // namespace dom
}  // namespace cobalt
