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

#include "cobalt/input/keypress_generator_filter.h"

#include "cobalt/dom/event_names.h"
#include "cobalt/input/keyboard_code.h"

namespace cobalt {
namespace input {

KeypressGeneratorFilter::KeypressGeneratorFilter(
    const KeyboardEventCallback& callback)
    : KeyEventHandler(callback) {}

KeypressGeneratorFilter::KeypressGeneratorFilter(KeyEventHandler* filter)
    : KeyEventHandler(filter) {}

void KeypressGeneratorFilter::HandleKeyboardEvent(
    const scoped_refptr<dom::KeyboardEvent>& keyboard_event) {
  // Handle the original event
  DispatchKeyboardEvent(keyboard_event);

  // If the event was a keydown, we may also generate a keypress event.
  ConditionallyGenerateKeypressEvent(keyboard_event);
}

bool KeypressGeneratorFilter::ConditionallyGenerateKeypressEvent(
    const scoped_refptr<dom::KeyboardEvent>& orig_event) {
  // Ignore everything but keydown events.
  if (orig_event->type() != dom::EventNames::GetInstance()->keydown()) {
    return false;
  }

  // Get the char_code corresponding to the key_code of the event.
  // Only generate the keypress event if there is a valid char_code.
  int key_code = orig_event->key_code();
  int char_code = KeyboardEventToCharCode(orig_event);

  if (char_code > 0) {
    scoped_refptr<dom::KeyboardEvent> keypress_event(new dom::KeyboardEvent(
        dom::EventNames::GetInstance()->keypress(),
        dom::KeyboardEvent::kDomKeyLocationStandard, orig_event->modifiers(),
        key_code, char_code, orig_event->repeat()));
    DispatchKeyboardEvent(keypress_event);
    return true;
  }

  return false;
}

// Static.
int KeypressGeneratorFilter::KeyboardEventToCharCode(
    const scoped_refptr<dom::KeyboardEvent>& event) {
  int key_code = event->key_code();

  // Keycodes unaffected by modifier keys.
  if (key_code == input::kBack || key_code == input::kReturn ||
      key_code == input::kSpace) {
    return key_code;
  }

  // No printable char if the Ctrl, Alt, etc. keys are held down
  if (event->alt_key() || event->ctrl_key() || event->meta_key()) {
    return 0;
  }

  if (event->shift_key()) {
    return KeyCodeToCharCodeWithShift(key_code);
  } else {
    return KeyCodeToCharCodeNoShift(key_code);
  }
}

// Static.
int KeypressGeneratorFilter::KeyCodeToCharCodeWithShift(int key_code) {
  // Characters are unaffected (keycode is uppercase by default).
  // Numbers map to the corresponding symbol.
  // Special symbols take on their shifted value.
  if (key_code >= input::kA && key_code <= input::kZ) {
    return key_code;
  }

  switch (key_code) {
    case input::k0:
      return ')';
    case input::k1:
      return '!';
    case input::k2:
      return '@';
    case input::k3:
      return '#';
    case input::k4:
      return '$';
    case input::k5:
      return '%';
    case input::k6:
      return '^';
    case input::k7:
      return '&';
    case input::k8:
      return '*';
    case input::k9:
      return '(';
    case input::kOem1:
      return ':';
    case input::kOemPlus:
      return '+';
    case input::kOemComma:
      return '<';
    case input::kOemMinus:
      return '_';
    case input::kOemPeriod:
      return '>';
    case input::kOem2:
      return '?';
    case input::kOem3:
      return '~';
    case input::kOem4:
      return '{';
    case input::kOem5:
      return '|';
    case input::kOem6:
      return '}';
    case input::kOem7:
      return '"';
    default:
      return 0;
  }
}

// Static.
int KeypressGeneratorFilter::KeyCodeToCharCodeNoShift(int key_code) {
  // Numbers are unaffected (keycode corresponds to correct Unicode value).
  // Characters are mapped from uppercase to lowercase.
  // Special symbols use their unshifted value.
  if (key_code >= input::k0 && key_code <= input::k9) {
    return key_code;
  }

  if (key_code >= input::kA && key_code <= input::kZ) {
    return key_code + 32;
  }

  switch (key_code) {
    case input::kOem1:
      return ';';
    case input::kOemPlus:
      return '=';
    case input::kOemComma:
      return ',';
    case input::kOemMinus:
      return '-';
    case input::kOemPeriod:
      return '.';
    case input::kOem2:
      return '/';
    case input::kOem3:
      return '`';
    case input::kOem4:
      return '[';
    case input::kOem5:
      return '\\';
    case input::kOem6:
      return ']';
    case input::kOem7:
      return '\'';
    default:
      return 0;
  }
}

}  // namespace input
}  // namespace cobalt
