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

#include "starboard/shared/input/key_charcode_conversion_internal.h"

#include "starboard/key.h"

namespace starboard {
namespace shared {
namespace input {

wchar_t KeyToCharCodeWithShift(SbKey key) {
  // Space is unaffected (keycode is same as Unicode).
  // Characters are unaffected (keycode is uppercase by default).
  // Numbers map to the corresponding symbol.
  // Special symbols take on their shifted value.
  if (key == SbKey::kSbKeySpace) {
    return key;
  }
  if (key >= SbKey::kSbKeyA && key <= SbKey::kSbKeyZ) {
    return key;
  }

  switch (key) {
    case SbKey::kSbKey0:
      return ')';
    case SbKey::kSbKey1:
      return '!';
    case SbKey::kSbKey2:
      return '@';
    case SbKey::kSbKey3:
      return '#';
    case SbKey::kSbKey4:
      return '$';
    case SbKey::kSbKey5:
      return '%';
    case SbKey::kSbKey6:
      return '^';
    case SbKey::kSbKey7:
      return '&';
    case SbKey::kSbKey8:
      return '*';
    case SbKey::kSbKey9:
      return '(';
    case SbKey::kSbKeyOem1:
      return ':';
    case SbKey::kSbKeyOemPlus:
      return '+';
    case SbKey::kSbKeyOemComma:
      return '<';
    case SbKey::kSbKeyOemMinus:
      return '_';
    case SbKey::kSbKeyOemPeriod:
      return '>';
    case SbKey::kSbKeyOem2:
      return '?';
    case SbKey::kSbKeyOem3:
      return '~';
    case SbKey::kSbKeyOem4:
      return '{';
    case SbKey::kSbKeyOem5:
      return '|';
    case SbKey::kSbKeyOem6:
      return '}';
    case SbKey::kSbKeyOem7:
      return '"';
    default:
      return 0;
  }
}

wchar_t KeyToCharCodeNoShift(SbKey key) {
  // Space keycode corresponds to Unicode value.
  // Numbers are unaffected (keycode corresponds to correct Unicode value).
  // Characters are mapped from uppercase to lowercase.
  // Special symbols use their unshifted value.
  if (key == SbKey::kSbKeySpace) {
    return key;
  }
  if (key >= SbKey::kSbKey0 && key <= SbKey::kSbKey9) {
    return key;
  }
  if (key >= SbKey::kSbKeyA && key <= SbKey::kSbKeyZ) {
    return key + 32;
  }

  switch (key) {
    case SbKey::kSbKeyOem1:
      return ';';
    case SbKey::kSbKeyOemPlus:
      return '=';
    case SbKey::kSbKeyOemComma:
      return ',';
    case SbKey::kSbKeyOemMinus:
      return '-';
    case SbKey::kSbKeyOemPeriod:
      return '.';
    case SbKey::kSbKeyOem2:
      return '/';
    case SbKey::kSbKeyOem3:
      return '`';
    case SbKey::kSbKeyOem4:
      return '[';
    case SbKey::kSbKeyOem5:
      return '\\';
    case SbKey::kSbKeyOem6:
      return ']';
    case SbKey::kSbKeyOem7:
      return '\'';
    default:
      return 0;
  }
}

wchar_t ComputeCharCode(SbKey key, uint32_t modifiers) {
  if (modifiers & SbKeyModifiers::kSbKeyModifiersShift) {
    return KeyToCharCodeWithShift(key);
  } else {
    return KeyToCharCodeNoShift(key);
  }
}

}  // namespace input
}  // namespace shared
}  // namespace starboard
