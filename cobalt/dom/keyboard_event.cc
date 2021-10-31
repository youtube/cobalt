// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/keyboard_event.h"

#include <string>

#include "base/logging.h"
#include "cobalt/base/token.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/keycode.h"

namespace cobalt {
namespace dom {

KeyboardEvent::KeyboardEvent(const std::string& type)
    : UIEventWithKeyState(base::Token(type), kBubbles, kCancelable, NULL),
      key_location_(kDomKeyLocationStandard),
      key_code_(0),
      char_code_(0),
      repeat_(false) {}

// TODO: Initialize from init_dict.key() or init_dict.code() when not empty.
KeyboardEvent::KeyboardEvent(const std::string& type,
                             const KeyboardEventInit& init_dict)
    : UIEventWithKeyState(base::Token(type), kBubbles, kCancelable,
                          init_dict.view(), init_dict),
      key_location_(static_cast<KeyLocationCode>(init_dict.location())),
      key_code_(init_dict.key_code()),
      char_code_(init_dict.char_code()),
      repeat_(init_dict.repeat()) {}

KeyboardEvent::KeyboardEvent(base::Token type,
                             const scoped_refptr<Window>& view,
                             const KeyboardEventInit& init_dict)
    : UIEventWithKeyState(type, kBubbles, kCancelable, view, init_dict),
      key_location_(static_cast<KeyLocationCode>(init_dict.location())),
      key_code_(init_dict.key_code()),
      char_code_(init_dict.char_code()),
      repeat_(init_dict.repeat()) {}

KeyboardEvent::KeyboardEvent(UninitializedFlag uninitialized_flag)
    : UIEventWithKeyState(uninitialized_flag),
      key_location_(kDomKeyLocationStandard),
      key_code_(0),
      char_code_(0),
      repeat_(false) {}

void KeyboardEvent::InitKeyboardEvent(const std::string& type, bool bubbles,
                                      bool cancelable,
                                      const scoped_refptr<Window>& view,
                                      const std::string& key, uint32 location,
                                      const std::string& modifierslist,
                                      bool repeat) {
  InitUIEventWithKeyState(type, bubbles, cancelable, view, 0, modifierslist);
  key_location_ = static_cast<KeyLocationCode>(location);
  repeat_ = repeat;

  // TODO: Implement full keyCode determination according to 'Legacy key models'
  // from: https://www.w3.org/TR/2016/WD-uievents-20160804/#legacy-key-models

  // Parse keys from the "Fixed virtual key codes" table from:
  // https://www.w3.org/TR/2016/WD-uievents-20160804/#fixed-virtual-key-codes
  if (key == "Backspace") {
    key_code_ = keycode::kBack;
  } else if (key == "Tab") {
    key_code_ = keycode::kTab;
  } else if (key == "Enter") {
    key_code_ = keycode::kReturn;
  } else if (key == "Shift") {
    key_code_ = keycode::kShift;
  } else if (key == "Control") {
    key_code_ = keycode::kControl;
  } else if (key == "Alt") {
    key_code_ = keycode::kMenu;
  } else if (key == "CapsLock") {
    key_code_ = keycode::kCapital;
  } else if (key == "Escape") {
    key_code_ = keycode::kEscape;
  } else if (key == "Space") {
    key_code_ = keycode::kSpace;
  } else if (key == "PageUp") {
    key_code_ = keycode::kPrior;
  } else if (key == "PageDown") {
    key_code_ = keycode::kNext;
  } else if (key == "End") {
    key_code_ = keycode::kEnd;
  } else if (key == "Home") {
    key_code_ = keycode::kHome;
  } else if (key == "ArrowLeft") {
    key_code_ = keycode::kLeft;
  } else if (key == "ArrowUp") {
    key_code_ = keycode::kUp;
  } else if (key == "ArrowRight") {
    key_code_ = keycode::kRight;
  } else if (key == "ArrowDown") {
    key_code_ = keycode::kDown;
  } else if (key == "Delete") {
    key_code_ = keycode::kDelete;
  }
}

// How to determine keycode:
//   https://www.w3.org/TR/2016/WD-uievents-20160804/#determine-keydown-keyup-keyCode
// Virtual key code for keyup/keydown, 0 for keypress (split model)
uint32 KeyboardEvent::key_code() const {
  if (type() == base::Tokens::keydown() || type() == base::Tokens::keyup()) {
    return key_code_;
  }

  return 0;
}

uint32 KeyboardEvent::char_code() const {
  return type() == base::Tokens::keypress() ? char_code_ : 0;
}

uint32 KeyboardEvent::which() const { return key_code(); }

std::string KeyboardEvent::NonPrintableKey(int32_t key_code) const {
  // Otherwise, we have one of the non-printable characters.
  // Definitions taken from:
  //   https://www.w3.org/TR/DOM-Level-3-Events-key/
  //   https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/key.
  switch (key_code) {
    case keycode::kBack:
      return "Backspace";
    case keycode::kTab:
      return "Tab";
    case keycode::kBacktab:
      return "Tab";
    case keycode::kClear:
      return "Clear";
    case keycode::kReturn:
      return "Enter";
    case keycode::kShift:
      return "Shift";
    case keycode::kControl:
      return "Control";
    case keycode::kMenu:
      return "Alt";
    case keycode::kPause:
      return "Pause";
    case keycode::kCapital:
      return "CapsLock";
    case keycode::kKana:
      return "KanaMode";
    case keycode::kJunja:
      return "JunjaMode";
    case keycode::kFinal:
      return "HanjaMode";
    case keycode::kKanji:
      return "KanjiMode";
    case keycode::kEscape:
      return "Esc";
    case keycode::kConvert:
      return "Convert";
    case keycode::kNonconvert:
      return "Nonconvert";
    case keycode::kAccept:
      return "Accept";
    case keycode::kModechange:
      return "ModeChange";
    case keycode::kSpace:
      return " ";
    case keycode::kPrior:
      return "PageUp";
    case keycode::kNext:
      return "PageDown";
    case keycode::kEnd:
      return "End";
    case keycode::kHome:
      return "Home";
    case keycode::kLeft:
      return "ArrowLeft";
    case keycode::kUp:
      return "ArrowUp";
    case keycode::kRight:
      return "ArrowRight";
    case keycode::kDown:
      return "ArrowDown";
    case keycode::kSelect:
      return "Select";
    case keycode::kPrint:
      return "Print";
    case keycode::kExecute:
      return "Execute";
    case keycode::kSnapshot:
      return "PrintScreen";
    case keycode::kInsert:
      return "Insert";
    case keycode::kDelete:
      return "Delete";
    case keycode::kHelp:
      return "Help";
    case keycode::kLwin:
      return "Win";
    case keycode::kRwin:
      return "Win";
    case keycode::kApps:
      return "Apps";
    case keycode::kSleep:
      return "Standby";
    case keycode::kNumpad0:
      return "0";
    case keycode::kNumpad1:
      return "1";
    case keycode::kNumpad2:
      return "2";
    case keycode::kNumpad3:
      return "3";
    case keycode::kNumpad4:
      return "4";
    case keycode::kNumpad5:
      return "5";
    case keycode::kNumpad6:
      return "6";
    case keycode::kNumpad7:
      return "7";
    case keycode::kNumpad8:
      return "8";
    case keycode::kNumpad9:
      return "9";
    case keycode::kMultiply:
      return "Multiply";
    case keycode::kAdd:
      return "Add";
    case keycode::kSeparator:
      return "Separator";
    case keycode::kSubtract:
      return "Subtract";
    case keycode::kDecimal:
      return "Decimal";
    case keycode::kDivide:
      return "Divide";
    case keycode::kF1:
      return "F1";
    case keycode::kF2:
      return "F2";
    case keycode::kF3:
      return "F3";
    case keycode::kF4:
      return "F4";
    case keycode::kF5:
      return "F5";
    case keycode::kF6:
      return "F6";
    case keycode::kF7:
      return "F7";
    case keycode::kF8:
      return "F8";
    case keycode::kF9:
      return "F9";
    case keycode::kF10:
      return "F10";
    case keycode::kF11:
      return "F11";
    case keycode::kF12:
      return "F12";
    case keycode::kF13:
      return "F13";
    case keycode::kF14:
      return "F14";
    case keycode::kF15:
      return "F15";
    case keycode::kF16:
      return "F16";
    case keycode::kF17:
      return "F17";
    case keycode::kF18:
      return "F18";
    case keycode::kF19:
      return "F19";
    case keycode::kF20:
      return "F20";
    case keycode::kF21:
      return "F21";
    case keycode::kF22:
      return "F22";
    case keycode::kF23:
      return "F23";
    case keycode::kF24:
      return "F24";
    case keycode::kNumlock:
      return "NumLock";
    case keycode::kScroll:
      return "Scroll";
    case keycode::kWlan:
      return "Unidentified";
    case keycode::kPower:
      return "Unidentified";
    case keycode::kLshift:
    case keycode::kRshift:
      return "Shift";
    case keycode::kLcontrol:
    case keycode::kRcontrol:
      return "Control";
    case keycode::kLmenu:
    case keycode::kRmenu:
      return "Alt";
    case keycode::kBrowserBack:
      return "BrowserBack";
    case keycode::kBrowserForward:
      return "BrowserForward";
    case keycode::kBrowserRefresh:
      return "BrowserRefresh";
    case keycode::kBrowserStop:
      return "BrowserStop";
    case keycode::kBrowserSearch:
      return "BrowserSearch";
    case keycode::kBrowserFavorites:
      return "BrowserFavorites";
    case keycode::kBrowserHome:
      return "BrowserHome";
    case keycode::kVolumeMute:
      return "VolumeMute";
    case keycode::kVolumeDown:
      return "VolumeMute";
    case keycode::kVolumeUp:
      return "VolumeMute";
    case keycode::kMediaNextTrack:
      return "MediaNextTrack";
    case keycode::kMediaPrevTrack:
      return "MediaPrevTrack";
    case keycode::kMediaStop:
      return "MediaStop";
    case keycode::kMediaPlayPause:
      return "MediaPlayPause";
    case keycode::kMediaLaunchMail:
      return "LaunchMail";
    case keycode::kMediaLaunchMediaSelect:
      return "SelectMedia";
    case keycode::kMediaLaunchApp1:
      return "LaunchApplication1";
    case keycode::kMediaLaunchApp2:
      return "LaunchApplication1";
    case keycode::kBrightnessDown:
      return "BrightnessDown";
    case keycode::kBrightnessUp:
      return "BrightnessUp";
    case keycode::kPlay:
      return "Play";
    default:
      return "Unidentified";
  }
}

std::string KeyboardEvent::key() const {
  // First check if the event corresponds to a printable character.
  // If so, just return a string containing that single character.
  int char_code = ComputeCharCode(key_code_, shift_key());
  if (char_code > 0 && char_code <= 127) {
    return std::string(1, static_cast<char>(char_code));
  }

  return NonPrintableKey(key_code_);
}

std::string KeyboardEvent::code() const {
  // First check if the event corresponds to a named key in the tables here:
  // https://www.w3.org/TR/uievents-code/#code-value-tables

  // First check if the event corresponds to a alphanumeric key.
  if (key_code_ >= keycode::kA && key_code_ <= keycode::kZ) {
    std::string key_code = "Key";
    key_code.append(1, static_cast<char>(key_code_));
    return key_code;
  }

  // Check if the event corresponds to a digit key.
  if (key_code_ >= keycode::k0 && key_code_ <= keycode::k9) {
    std::string key_code = "Digit";
    key_code.append(1, static_cast<char>(key_code_));
    return key_code;
  }

  // Check if the event corresponds to a numpad digit key.
  //  https://www.w3.org/TR/uievents-code/#key-numpad-section
  if (key_code_ >= keycode::kNumpad0 && key_code_ <= keycode::kDivide) {
    std::string key_code = "Numpad";
    char numpad_digit = '0' + static_cast<char>(key_code_ - keycode::kNumpad0);
    key_code.append(1, numpad_digit);
    return key_code;
  }

  // Check if the event corresponds to a remaining named key
  switch (key_code_) {
    case keycode::kSpace:
      return "Space";
    case keycode::kMultiply:
      return "NumpadMultiply";
    case keycode::kAdd:
      return "NumpadAdd";
    case keycode::kSeparator:
      return "NumpadComma";
    case keycode::kSubtract:
      return "NumpadSubtract";
    case keycode::kDecimal:
      return "NumpadDecimal";
    case keycode::kDivide:
      return "NumpadDivide";
    case keycode::kOem1:
      return "Semicolon";
    case keycode::kOemPlus:
      return "Equal";
    case keycode::kOemComma:
      return "Comma";
    case keycode::kOemMinus:
      return "Minus";
    case keycode::kOemPeriod:
      return "Period";
    case keycode::kOem2:
      return "Slash";
    case keycode::kOem3:
      return "Backquote";
    case keycode::kOem4:
      return "BracketLeft";
    case keycode::kOem5:
      return "Backslash";
    case keycode::kOem6:
      return "BracketRight";
    case keycode::kOem7:
      return "Quote";
    case keycode::kLwin:
      return "MetaLeft";
    case keycode::kRwin:
      return "MetaRight";
    case keycode::kLshift:
      return "ShiftLeft";
    case keycode::kRshift:
      return "ShiftRight";
    case keycode::kLcontrol:
      return "ControlLeft";
    case keycode::kRcontrol:
      return "ControlRight";
    case keycode::kLmenu:
      return "AltLeft";
    case keycode::kRmenu:
      return "AltRight";
    default:
      // Nothing.
      break;
  }

  // The event corresponds to a nonprintable key with a name that matches the
  // named value for the key.
  return NonPrintableKey(key_code_);
}

// Static.
int32 KeyboardEvent::ComputeCharCode(int32 key_code, bool shift_key) {
  if (shift_key) {
    return KeyCodeToCharCodeWithShift(key_code);
  } else {
    return KeyCodeToCharCodeNoShift(key_code);
  }
}

// Static.
int KeyboardEvent::KeyCodeToCharCodeWithShift(int key_code) {
  // Space is unaffected (keycode is same as Unicode).
  // Characters are unaffected (keycode is uppercase by default).
  // Numbers map to the corresponding symbol.
  // Special symbols take on their shifted value.
  if (key_code == keycode::kSpace) {
    return key_code;
  }
  if (key_code >= keycode::kA && key_code <= keycode::kZ) {
    return key_code;
  }

  switch (key_code) {
    case keycode::k0:
      return ')';
    case keycode::k1:
      return '!';
    case keycode::k2:
      return '@';
    case keycode::k3:
      return '#';
    case keycode::k4:
      return '$';
    case keycode::k5:
      return '%';
    case keycode::k6:
      return '^';
    case keycode::k7:
      return '&';
    case keycode::k8:
      return '*';
    case keycode::k9:
      return '(';
    case keycode::kOem1:
      return ':';
    case keycode::kOemPlus:
      return '+';
    case keycode::kOemComma:
      return '<';
    case keycode::kOemMinus:
      return '_';
    case keycode::kOemPeriod:
      return '>';
    case keycode::kOem2:
      return '?';
    case keycode::kOem3:
      return '~';
    case keycode::kOem4:
      return '{';
    case keycode::kOem5:
      return '|';
    case keycode::kOem6:
      return '}';
    case keycode::kOem7:
      return '"';
    default:
      return 0;
  }
}

// Static.
int KeyboardEvent::KeyCodeToCharCodeNoShift(int key_code) {
  // Space keycode corresponds to Unicode value.
  // Numbers are unaffected (keycode corresponds to correct Unicode value).
  // Characters are mapped from uppercase to lowercase.
  // Special symbols use their unshifted value.
  if (key_code == keycode::kSpace) {
    return key_code;
  }
  if (key_code >= keycode::k0 && key_code <= keycode::k9) {
    return key_code;
  }
  if (key_code >= keycode::kA && key_code <= keycode::kZ) {
    return key_code + 32;
  }

  switch (key_code) {
    case keycode::kOem1:
      return ';';
    case keycode::kOemPlus:
      return '=';
    case keycode::kOemComma:
      return ',';
    case keycode::kOemMinus:
      return '-';
    case keycode::kOemPeriod:
      return '.';
    case keycode::kOem2:
      return '/';
    case keycode::kOem3:
      return '`';
    case keycode::kOem4:
      return '[';
    case keycode::kOem5:
      return '\\';
    case keycode::kOem6:
      return ']';
    case keycode::kOem7:
      return '\'';
    default:
      return 0;
  }
}

// Static.
KeyboardEvent::KeyLocationCode KeyboardEvent::KeyCodeToKeyLocation(
    int key_code) {
  switch (key_code) {
    case keycode::kLshift:
    case keycode::kLcontrol:
    case keycode::kLmenu:
    case keycode::kLwin:
      return kDomKeyLocationLeft;
    case keycode::kRshift:
    case keycode::kRcontrol:
    case keycode::kRmenu:
    case keycode::kRwin:
      return kDomKeyLocationRight;
    default:
      return kDomKeyLocationStandard;
  }
}

}  // namespace dom
}  // namespace cobalt
