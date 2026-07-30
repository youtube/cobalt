// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_STARBOARD_H_
#define UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_STARBOARD_H_

#include "starboard/key.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

DomCode SbKeyToDomCode(SbKey sb_key);
DomKey SbKeyToDomKey(SbKey sb_key, bool shift);
KeyboardCode SbKeyToKeyboardCode(SbKey sb_key);

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_STARBOARD_H_
