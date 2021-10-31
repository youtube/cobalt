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

#include "cobalt/dom/ui_event_with_key_state.h"

#include <vector>

#include "base/strings/string_split.h"
#include "cobalt/system_window/input_event.h"

namespace cobalt {
namespace dom {

void UIEventWithKeyState::InitUIEventWithKeyState(
    const std::string& type, bool bubbles, bool cancelable,
    const scoped_refptr<Window>& view, int32 detail,
    const std::string& modifierslist) {
  InitUIEvent(type, bubbles, cancelable, view, detail);

  ctrl_key_ = false;
  shift_key_ = false;
  alt_key_ = false;
  meta_key_ = false;

  std::vector<std::string> modifiers =
      base::SplitString(modifierslist, base::kWhitespaceASCII,
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (std::vector<std::string>::const_iterator it = modifiers.begin();
       it != modifiers.end(); ++it) {
    const std::string& modifier = *it;
    if (modifier == "Alt") {
      alt_key_ = true;
    } else if (modifier == "Control") {
      ctrl_key_ = true;
    } else if (modifier == "Meta") {
      meta_key_ = true;
    } else if (modifier == "Shift") {
      shift_key_ = true;
    }
  }
}

void UIEventWithKeyState::InitUIEventWithKeyState(
    const std::string& type, bool bubbles, bool cancelable,
    const scoped_refptr<Window>& view, int32 detail, bool ctrl_key,
    bool alt_key, bool shift_key, bool meta_key) {
  InitUIEvent(type, bubbles, cancelable, view, detail);

  ctrl_key_ = ctrl_key;
  shift_key_ = shift_key;
  alt_key_ = alt_key;
  meta_key_ = meta_key;
}

bool UIEventWithKeyState::GetModifierState(const std::string& keyArg) const {
  // Standard names of modifier keys defined here:
  // https://www.w3.org/TR/DOM-Level-3-Events-key/#keys-modifier
  if (keyArg == "Alt") {
    return alt_key();
  } else if (keyArg == "Control") {
    return ctrl_key();
  } else if (keyArg == "Meta") {
    return meta_key();
  } else if (keyArg == "Shift") {
    return shift_key();
  }
  return false;
}

}  // namespace dom
}  // namespace cobalt
