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

#include "cobalt/dom/mouse_event.h"

#include <string>

#include "cobalt/base/token.h"

namespace cobalt {
namespace dom {

MouseEvent::MouseEvent(const std::string& type)
    : UIEventWithKeyState(base::Token(type), kBubbles, kCancelable, NULL),
      screen_x_(0),
      screen_y_(0),
      client_x_(0),
      client_y_(0),
      button_(0),
      buttons_(0) {}

MouseEvent::MouseEvent(const std::string& type, const MouseEventInit& init_dict)
    : UIEventWithKeyState(base::Token(type), kBubbles, kCancelable,
                          init_dict.view(), init_dict),
      screen_x_(init_dict.screen_x()),
      screen_y_(init_dict.screen_y()),
      client_x_(init_dict.client_x()),
      client_y_(init_dict.client_y()),
      button_(init_dict.button()),
      buttons_(init_dict.buttons()) {}

MouseEvent::MouseEvent(base::Token type, const scoped_refptr<Window>& view,
                       const MouseEventInit& init_dict)
    : UIEventWithKeyState(type, kBubbles, kCancelable, view, init_dict),
      screen_x_(init_dict.screen_x()),
      screen_y_(init_dict.screen_y()),
      client_x_(init_dict.client_x()),
      client_y_(init_dict.client_y()),
      button_(init_dict.button()),
      buttons_(init_dict.buttons()) {}

MouseEvent::MouseEvent(base::Token type, Bubbles bubbles, Cancelable cancelable,
                       const scoped_refptr<Window>& view,
                       const MouseEventInit& init_dict)
    : UIEventWithKeyState(type, bubbles, cancelable, view, init_dict),
      screen_x_(init_dict.screen_x()),
      screen_y_(init_dict.screen_y()),
      client_x_(init_dict.client_x()),
      client_y_(init_dict.client_y()),
      button_(init_dict.button()),
      buttons_(init_dict.buttons()) {}

MouseEvent::MouseEvent(UninitializedFlag uninitialized_flag)
    : UIEventWithKeyState(uninitialized_flag),
      screen_x_(0),
      screen_y_(0),
      client_x_(0),
      client_y_(0),
      button_(0),
      buttons_(0) {}

void MouseEvent::InitMouseEvent(
    const std::string& type, bool bubbles, bool cancelable,
    const scoped_refptr<Window>& view, int32 detail, int32 screen_x,
    int32 screen_y, int32 client_x, int32 client_y, bool ctrl_key, bool alt_key,
    bool shift_key, bool meta_key, uint16 button,
    const scoped_refptr<EventTarget>& related_target) {
  InitUIEventWithKeyState(type, bubbles, cancelable, view, detail, ctrl_key,
                          alt_key, shift_key, meta_key);
  screen_x_ = screen_x;
  screen_y_ = screen_y;
  client_x_ = client_x;
  client_y_ = client_y;
  button_ = button;
  buttons_ = 0;
  related_target_ = related_target;
}

void MouseEvent::InitMouseEvent(
    const std::string& type, bool bubbles, bool cancelable,
    const scoped_refptr<Window>& view, int32 detail, int32 screen_x,
    int32 screen_y, int32 client_x, int32 client_y,
    const std::string& modifierslist, uint16 button,
    const scoped_refptr<EventTarget>& related_target) {
  InitUIEventWithKeyState(type, bubbles, cancelable, view, detail,
                          modifierslist);
  screen_x_ = screen_x;
  screen_y_ = screen_y;
  client_x_ = client_x;
  client_y_ = client_y;
  button_ = button;
  buttons_ = 0;
  related_target_ = related_target;
}

}  // namespace dom
}  // namespace cobalt
