// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BROWSER_ON_SCREEN_KEYBOARD_STARBOARD_BRIDGE_H_
#define COBALT_BROWSER_ON_SCREEN_KEYBOARD_STARBOARD_BRIDGE_H_

#include "base/callback.h"
#include "cobalt/dom/on_screen_keyboard_bridge.h"
#include "starboard/window.h"

#if SB_HAS(ON_SCREEN_KEYBOARD)
namespace cobalt {
namespace browser {

// OnScreenKeyboardStarboardBridge implements the OnScreenKeyboardBridge,
// calling Starboard methods to control the OnScreenKeyboard. The constructor
// takes in a callback provider for the SbWindow.
class OnScreenKeyboardStarboardBridge : public dom::OnScreenKeyboardBridge {
 public:
  explicit OnScreenKeyboardStarboardBridge(
      const base::Callback<SbWindow()>& sb_window_provider)
      : sb_window_provider_(sb_window_provider) {
    DCHECK(!sb_window_provider_.is_null());
  }

  void Show(const char* input_text, int ticket) override;

  void Hide(int ticket) override;

  void Focus(int ticket) override;

  void Blur(int ticket) override;

  bool IsShown() const override;

  bool IsValidTicket(int ticket) const override;

  void SetKeepFocus(bool keep_focus) override;

 private:
  base::Callback<SbWindow()> sb_window_provider_;
};

}  // namespace browser
}  // namespace cobalt
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
#endif  // COBALT_BROWSER_ON_SCREEN_KEYBOARD_STARBOARD_BRIDGE_H_
