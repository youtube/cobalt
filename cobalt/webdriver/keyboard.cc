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

#include "cobalt/webdriver/keyboard.h"

#include <limits>

#include "base/i18n/char_iterator.h"
#include "cobalt/base/token.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/keycode.h"

using cobalt::dom::KeyboardEvent;

namespace cobalt {
namespace webdriver {
namespace {

typedef KeyboardEvent::KeyLocationCode KeyLocationCode;

// The following utf-8 code points could be provided as "keys" sent to
// WebDriver, and should be mapped to the corresponding keyboard code.
enum SpecialKey {
  kFirstSpecialKey = 0xE000,
  kSpecialKey_Null = kFirstSpecialKey,
  kSpecialKey_Cancel,
  kSpecialKey_Help,
  kSpecialKey_Backspace,
  kSpecialKey_Tab,
  kSpecialKey_Clear,
  kSpecialKey_Return,
  kSpecialKey_Enter,
  kSpecialKey_Shift,
  kSpecialKey_Ctrl,
  kSpecialKey_Alt,
  kSpecialKey_Pause,
  kSpecialKey_Escape,
  kSpecialKey_Space,
  kSpecialKey_Pageup,
  kSpecialKey_Pagedown,
  kSpecialKey_End,
  kSpecialKey_Home,
  kSpecialKey_LeftArrow,
  kSpecialKey_UpArrow,
  kSpecialKey_RightArrow,
  kSpecialKey_DownArrow,
  kSpecialKey_Insert,
  kSpecialKey_Delete,
  kSpecialKey_Semicolon,
  kSpecialKey_Equals,
  kSpecialKey_Numpad0,
  kSpecialKey_Numpad1,
  kSpecialKey_Numpad2,
  kSpecialKey_Numpad3,
  kSpecialKey_Numpad4,
  kSpecialKey_Numpad5,
  kSpecialKey_Numpad6,
  kSpecialKey_Numpad7,
  kSpecialKey_Numpad8,
  kSpecialKey_Numpad9,
  kSpecialKey_Multiply,
  kSpecialKey_Add,
  kSpecialKey_Separator,
  kSpecialKey_Subtract,
  kSpecialKey_Decimal,
  kSpecialKey_Divide,  // = 0xE029
  // 0xE02A to 0xE030 are not mapped to anything.
  kSpecialKey_F1 = 0xE031,  //
  kSpecialKey_F2,
  kSpecialKey_F3,
  kSpecialKey_F4,
  kSpecialKey_F5,
  kSpecialKey_F6,
  kSpecialKey_F7,
  kSpecialKey_F8,
  kSpecialKey_F9,
  kSpecialKey_F10,
  kSpecialKey_F11,
  kSpecialKey_F12,
  kSpecialKey_Meta,
  kLastSpecialKey = kSpecialKey_Meta
};
// Assert the expected values.
COMPILE_ASSERT(kSpecialKey_Divide == 0xE029, MissingAnEnum);
COMPILE_ASSERT(kLastSpecialKey == 0xE03D, MissingAnEnum);

// Mapping from a special keycode to virtual keycode. Subtract kFirstSpecialKey
// from the integer value of the WebDriver keycode and index into this table.
const int32 special_keycode_mapping[] = {
    dom::keycode::kUnknown,    // kSpecialKey_NULL
    dom::keycode::kCancel,     // kSpecialKey_Cancel,
    dom::keycode::kHelp,       // kSpecialKey_Help
    dom::keycode::kBack,       // kSpecialKey_Backspace,
    dom::keycode::kTab,        // kSpecialKey_Tab
    dom::keycode::kClear,      // kSpecialKey_Clear
    dom::keycode::kReturn,     // kSpecialKey_Return
    dom::keycode::kReturn,     // kSpecialKey_Enter (on numeric keypad)
    dom::keycode::kShift,      // kSpecialKey_Shift
    dom::keycode::kControl,    // kSpecialKey_Control
    dom::keycode::kMenu,       // kSpecialKey_Alt
    dom::keycode::kPause,      // kSpecialKey_Pause
    dom::keycode::kEscape,     // kSpecialKey_Escape
    dom::keycode::kSpace,      // kSpecialKey_Space
    dom::keycode::kPrior,      // kSpecialKey_Pageup,
    dom::keycode::kNext,       // kSpecialKey_Pagedown,
    dom::keycode::kEnd,        // kSpecialKey_End
    dom::keycode::kHome,       // kSpecialKey_Home
    dom::keycode::kLeft,       // kSpecialKey_LeftArrow,
    dom::keycode::kUp,         // kSpecialKey_UpArrow,
    dom::keycode::kRight,      // kSpecialKey_RightArrow,
    dom::keycode::kDown,       // kSpecialKey_DownArrow,
    dom::keycode::kInsert,     // kSpecialKey_Insert
    dom::keycode::kDelete,     // kSpecialKey_Delete
    dom::keycode::kOem1,       // kSpecialKey_Semicolon,
    dom::keycode::kOemPlus,    // kSpecialKey_Equals (on numeric keypad)
    dom::keycode::kNumpad0,    // kSpecialKey_Numpad0
    dom::keycode::kNumpad1,    // kSpecialKey_Numpad1
    dom::keycode::kNumpad2,    // kSpecialKey_Numpad2
    dom::keycode::kNumpad3,    // kSpecialKey_Numpad3
    dom::keycode::kNumpad4,    // kSpecialKey_Numpad4
    dom::keycode::kNumpad5,    // kSpecialKey_Numpad5
    dom::keycode::kNumpad6,    // kSpecialKey_Numpad6
    dom::keycode::kNumpad7,    // kSpecialKey_Numpad7
    dom::keycode::kNumpad8,    // kSpecialKey_Numpad8
    dom::keycode::kNumpad9,    // kSpecialKey_Numpad9
    dom::keycode::kMultiply,   // kSpecialKey_Multiply,
    dom::keycode::kAdd,        // kSpecialKey_Add
    dom::keycode::kSeparator,  // kSpecialKey_Separator
    dom::keycode::kSubtract,   // kSpecialKey_Subtract
    dom::keycode::kDecimal,    // kSpecialKey_Decimal
    dom::keycode::kDivide,     // kSpecialKey_Divide = 0xE029
    dom::keycode::kUnknown,    // 0xE02A
    dom::keycode::kUnknown,    // 0xE02B
    dom::keycode::kUnknown,    // 0xE02C
    dom::keycode::kUnknown,    // 0xE02D
    dom::keycode::kUnknown,    // 0xE02E
    dom::keycode::kUnknown,    // 0xE02F
    dom::keycode::kUnknown,    // 0xE030
    dom::keycode::kF1,         // kSpecialKey_F1 = 0xE031
    dom::keycode::kF2,         // kSpecialKey_F2
    dom::keycode::kF3,         // kSpecialKey_F3
    dom::keycode::kF4,         // kSpecialKey_F4
    dom::keycode::kF5,         // kSpecialKey_F5
    dom::keycode::kF6,         // kSpecialKey_F6
    dom::keycode::kF7,         // kSpecialKey_F7
    dom::keycode::kF8,         // kSpecialKey_F8
    dom::keycode::kF9,         // kSpecialKey_F9
    dom::keycode::kF10,        // kSpecialKey_F10
    dom::keycode::kF11,        // kSpecialKey_F11
    dom::keycode::kF12,        // kSpecialKey_F12
    dom::keycode::kLwin,       // kSpecialKey_Meta
};

// Besides of selenium defined special keys, we need additional special keys
// for media control. The following utf-8 code could be provided as "keys"
// sent to WebDriver, and should be mapped to the corresponding keyboard code.
enum AdditionalSpecialKey {
  kFirstAdditionalSpecialKey = 0xF000,
  kSpecialKey_MediaNextTrack = kFirstAdditionalSpecialKey,
  kSpecialKey_MediaPrevTrack,
  kSpecialKey_MediaStop,
  kSpecialKey_MediaPlayPause,
  kSpecialKey_MediaRewind,
  kSpecialKey_MediaFastForward,
  kLastAdditionalSpecialKey = kSpecialKey_MediaFastForward,
};

// Mapping from an additional special keycode to virtual keycode. Subtract
// kFirstAdditionalSpecialKey from the integer value of the WebDriver keycode
// and index into this table.
const int32 additional_special_keycode_mapping[] = {
    dom::keycode::kMediaNextTrack,    // kMediaNextTrack,
    dom::keycode::kMediaPrevTrack,    // kMediaPrevTrack,
    dom::keycode::kMediaStop,         // kMediaStop,
    dom::keycode::kMediaPlayPause,    // kMediaPlayPause,
    dom::keycode::kMediaRewind,       // kMediaRewind,
    dom::keycode::kMediaFastForward,  // kMediaFastForward,
};

// Check that the mapping is the expected size.
const int kLargestMappingIndex = kLastSpecialKey - kFirstSpecialKey;
COMPILE_ASSERT(arraysize(special_keycode_mapping) == kLargestMappingIndex + 1,
               IncorrectMappingTable);

// Translate a utf8 character to a keycode. Characters that would require the
// shift key to be pressed are lower-cased or translated to their corresponding
// non-shifted special character.
// This is based on a standard US keyboard.
int CharacterToKeyCode(char character) {
  if (character >= '0' && character <= '9') {
    return character;
  }
  if (character >= 'a' && character <= 'z') {
    return character - 32;
  }
  if (character >= 'A' && character <= 'Z') {
    return character;
  }

  switch (character) {
    case ' ':
      return dom::keycode::kSpace;
    case ')':
      return dom::keycode::k0;
    case '!':
      return dom::keycode::k1;
    case '@':
      return dom::keycode::k2;
    case '#':
      return dom::keycode::k3;
    case '$':
      return dom::keycode::k4;
    case '%':
      return dom::keycode::k5;
    case '^':
      return dom::keycode::k6;
    case '&':
      return dom::keycode::k7;
    case '*':
      return dom::keycode::k8;
    case '(':
      return dom::keycode::k9;

    case ':':
    case ';':
      return dom::keycode::kOem1;
    case '+':
    case '=':
      return dom::keycode::kOemPlus;
    case '<':
    case ',':
      return dom::keycode::kOemComma;
    case '_':
    case '-':
      return dom::keycode::kOemMinus;
    case '>':
    case '.':
      return dom::keycode::kOemPeriod;
    case '?':
    case '/':
      return dom::keycode::kOem2;
    case '~':
    case '`':
      return dom::keycode::kOem3;
    case '{':
    case '[':
      return dom::keycode::kOem4;
    case '|':
    case '\\':
      return dom::keycode::kOem5;
    case '}':
    case ']':
      return dom::keycode::kOem6;
    case '"':
    case '\'':
      return dom::keycode::kOem7;
  }
  NOTREACHED();
  return 0;
}

// Returns true iff this utf8 codepoint corresponds to a WebDriver special key.
bool IsSpecialKey(int webdriver_key) {
  return webdriver_key >= kFirstSpecialKey && webdriver_key < kLastSpecialKey;
}

bool IsAdditionalSpecialKey(int webdriver_key) {
  return webdriver_key >= kFirstAdditionalSpecialKey &&
         webdriver_key <= kLastAdditionalSpecialKey;
}

bool IsModifierKey(int webdriver_key) {
  return webdriver_key == kSpecialKey_Alt ||
         webdriver_key == kSpecialKey_Shift ||
         webdriver_key == kSpecialKey_Ctrl;
}

// Returns true if typing the utf8 character on a standard US keyboard would
// require the shift modifier to be pressed.
bool CharacterRequiresShift(char character) {
  DCHECK_NE(0, character);
  const char kSpecialsRequiringShift[] = ")!@#$%%^&*(:+<_>?~{|}\"}";
  if (character >= 'A' && character <= 'Z') {
    return true;
  }
  return (strchr(kSpecialsRequiringShift, character) != NULL);
}

// Returns the keycode that corresponds to this WebDriver special key.
int32 GetSpecialKeycode(int32 webdriver_key) {
  DCHECK(IsSpecialKey(webdriver_key));
  int index = webdriver_key - kFirstSpecialKey;
  DCHECK_GE(index, 0);
  DCHECK_LT(index, arraysize(special_keycode_mapping));
  return special_keycode_mapping[index];
}

// Returns the location code for the key.
KeyLocationCode GetSpecialKeyLocation(int32 webdriver_key) {
  DCHECK(IsSpecialKey(webdriver_key));
  if ((webdriver_key >= kSpecialKey_Equals &&
       webdriver_key <= kSpecialKey_Divide) ||
      webdriver_key == kSpecialKey_Enter ||
      webdriver_key == kSpecialKey_Equals) {
    return KeyboardEvent::kDomKeyLocationNumpad;
  }
  if (webdriver_key == kSpecialKey_Shift || webdriver_key == kSpecialKey_Ctrl ||
      webdriver_key == kSpecialKey_Alt || webdriver_key == kSpecialKey_Meta) {
    // Choose all of these are the left ones. WebDriver doesn't distinguish
    // between left and right modifiers.
    return KeyboardEvent::kDomKeyLocationLeft;
  }
  // Other keys do not have a special location.
  return KeyboardEvent::kDomKeyLocationStandard;
}

// Returns the keycode that corresponds to this additional special key.
int32 GetAdditionalSpecialKeycode(int32 webdriver_key) {
  DCHECK(IsAdditionalSpecialKey(webdriver_key));
  int index = webdriver_key - kFirstAdditionalSpecialKey;
  DCHECK_GE(index, 0);
  DCHECK_LT(index, arraysize(additional_special_keycode_mapping));
  return additional_special_keycode_mapping[index];
}

class KeyTranslator {
 public:
  explicit KeyTranslator(Keyboard::KeyboardEventVector* event_vector)
      : shift_pressed_(false),
        ctrl_pressed_(false),
        alt_pressed_(false),
        event_vector_(event_vector) {}

  // Runs the sendKeys() algorithm to translate utf8 code points into key
  // press events.
  // https://www.w3.org/TR/webdriver/#sendkeys
  void Translate(const std::string& utf8_keys) {
    base::i18n::UTF8CharIterator utf8_iterator(&utf8_keys);
    while (!utf8_iterator.end()) {
      int32 webdriver_key = utf8_iterator.get();
      utf8_iterator.Advance();
      if (webdriver_key == kSpecialKey_Null) {
        // If it's a NULL key, release the modifiers.
        ReleaseModifiers();
        DCHECK(!shift_pressed_);
        DCHECK(!alt_pressed_);
        DCHECK(!ctrl_pressed_);
      } else if (IsModifierKey(webdriver_key)) {
        // Else if it's a modifier, toggle the modifier state.
        ToggleModifier(webdriver_key);
      } else if (IsSpecialKey(webdriver_key)) {
        // Else if it's a WebDriver special key, translate to key_code and
        // send key events.
        int32 key_code = GetSpecialKeycode(webdriver_key);
        int32 char_code = 0;
        KeyLocationCode location = GetSpecialKeyLocation(webdriver_key);
        AddKeyDownEvent(key_code, char_code, location);
        AddKeyUpEvent(key_code, char_code, location);
      } else if (IsAdditionalSpecialKey(webdriver_key)) {
        // Else if it's an additional special key, translate to key_code and
        // send key events.
        int32 key_code = GetAdditionalSpecialKeycode(webdriver_key);
        int32 char_code = 0;
        KeyLocationCode location = KeyboardEvent::kDomKeyLocationStandard;
        AddKeyDownEvent(key_code, char_code, location);
        AddKeyUpEvent(key_code, char_code, location);
      } else {
        DCHECK_GE(webdriver_key, 0);
        DCHECK_LT(webdriver_key, std::numeric_limits<char>::max());
        const char character = static_cast<char>(webdriver_key);
        int key_code = CharacterToKeyCode(character);
        KeyLocationCode location = KeyboardEvent::kDomKeyLocationStandard;
        if (CharacterRequiresShift(character)) {
          // Handle Upper-case characters. Press and release Shift if it's not
          // being held already.
          bool shift_was_pressed = shift_pressed_;
          if (!shift_was_pressed) {
            ToggleModifier(kSpecialKey_Shift);
          }
          int32 char_code = KeyboardEvent::KeyCodeToCharCodeWithShift(key_code);
          DCHECK_EQ(char_code, character);

          AddKeyDownEvent(key_code, char_code, location);
          AddKeyPressEvent(key_code, char_code, location);
          AddKeyUpEvent(key_code, char_code, location);

          if (!shift_was_pressed) {
            ToggleModifier(kSpecialKey_Shift);
          }
        } else {
          // Handle lower-case characters. If shift is pressed, convert the
          // character to uppercase.
          int32 char_code = KeyboardEvent::KeyCodeToCharCodeNoShift(key_code);
          DCHECK_EQ(char_code, character);
          if (shift_pressed_) {
            char_code = KeyboardEvent::KeyCodeToCharCodeWithShift(key_code);
          }
          AddKeyDownEvent(key_code, char_code, location);
          AddKeyPressEvent(key_code, char_code, location);
          AddKeyUpEvent(key_code, char_code, location);
        }
      }
    }
  }

  void ReleaseModifiers() {
    if (alt_pressed_) {
      ToggleModifier(kSpecialKey_Alt);
    }
    if (ctrl_pressed_) {
      ToggleModifier(kSpecialKey_Ctrl);
    }
    if (shift_pressed_) {
      ToggleModifier(kSpecialKey_Shift);
    }
  }

 private:
  void ToggleModifier(int webdriver_key) {
    DCHECK(IsModifierKey(webdriver_key));
    bool do_keyup = false;
    if (webdriver_key == kSpecialKey_Alt) {
      do_keyup = alt_pressed_;
      alt_pressed_ = !alt_pressed_;
    } else if (webdriver_key == kSpecialKey_Shift) {
      do_keyup = shift_pressed_;
      shift_pressed_ = !shift_pressed_;
    } else if (webdriver_key == kSpecialKey_Ctrl) {
      do_keyup = ctrl_pressed_;
      ctrl_pressed_ = !ctrl_pressed_;
    } else {
      NOTREACHED();
    }
    DCHECK(IsSpecialKey(webdriver_key));
    int32 char_code = 0;
    int32 key_code = GetSpecialKeycode(webdriver_key);
    KeyLocationCode location = GetSpecialKeyLocation(webdriver_key);
    if (do_keyup) {
      AddKeyUpEvent(key_code, char_code, location);
    } else {
      AddKeyDownEvent(key_code, char_code, location);
    }
  }

  void AddKeyDownEvent(int key_code, int char_code, KeyLocationCode location) {
    AddKeyEvent(base::Tokens::keydown(), key_code, char_code, location);
  }

  void AddKeyPressEvent(int key_code, int char_code, KeyLocationCode location) {
    AddKeyEvent(base::Tokens::keypress(), key_code, char_code, location);
  }

  void AddKeyUpEvent(int key_code, int char_code, KeyLocationCode location) {
    AddKeyEvent(base::Tokens::keyup(), key_code, char_code, location);
  }

  void AddKeyEvent(base::Token type, int key_code, int char_code,
                   KeyLocationCode location) {
    dom::KeyboardEventInit event;
    event.set_location(location);
    event.set_shift_key(shift_pressed_);
    event.set_ctrl_key(ctrl_pressed_);
    event.set_alt_key(alt_pressed_);
    event.set_key_code(key_code);
    event.set_char_code(char_code);
    event.set_repeat(false);
    event_vector_->push_back(std::make_pair(type, event));
  }

  bool shift_pressed_;
  bool ctrl_pressed_;
  bool alt_pressed_;
  Keyboard::KeyboardEventVector* event_vector_;
};

}  // namespace

void Keyboard::TranslateToKeyEvents(const std::string& utf8_keys,
                                    TerminationBehaviour termination_behaviour,
                                    KeyboardEventVector* out_events) {
  KeyTranslator translator(out_events);
  translator.Translate(utf8_keys);
  if (termination_behaviour == kReleaseModifiers) {
    translator.ReleaseModifiers();
  }
}

}  // namespace webdriver
}  // namespace cobalt
