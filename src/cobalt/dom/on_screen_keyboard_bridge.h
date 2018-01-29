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

#ifndef COBALT_DOM_ON_SCREEN_KEYBOARD_BRIDGE_H_
#define COBALT_DOM_ON_SCREEN_KEYBOARD_BRIDGE_H_

namespace cobalt {
namespace dom {

// OnScreenKeyboardBridge defines virtual methods that control the
// OnScreenKeyboard. These functions are called directly from the the thread
// that the Web API is running on.
class OnScreenKeyboardBridge {
 public:
  virtual ~OnScreenKeyboardBridge() {}
  virtual void Show(const char* input_text, int ticket) = 0;
  virtual void Hide(int ticket) = 0;
  virtual void Focus(int ticket) = 0;
  virtual void Blur(int ticket) = 0;
  virtual bool IsShown() const = 0;
  virtual void SetKeepFocus(bool keep_focus) = 0;
  virtual bool IsValidTicket(int ticket) const = 0;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_ON_SCREEN_KEYBOARD_BRIDGE_H_
