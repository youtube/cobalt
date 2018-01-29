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

#ifndef COBALT_DOM_MOUSE_EVENT_H_
#define COBALT_DOM_MOUSE_EVENT_H_

#include <string>

#include "cobalt/base/token.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/mouse_event_init.h"
#include "cobalt/dom/ui_event_with_key_state.h"

namespace cobalt {
namespace dom {

// The MouseEvent provides specific contextual information associated with
// mouse devices.
//   https://www.w3.org/TR/2016/WD-uievents-20160804/#events-mouseevents
class MouseEvent : public UIEventWithKeyState {
 public:
  explicit MouseEvent(const std::string& type);
  MouseEvent(const std::string& type, const MouseEventInit& init_dict);
  MouseEvent(base::Token type, const scoped_refptr<Window>& view,
             const MouseEventInit& init_dict);
  MouseEvent(base::Token type, Bubbles bubbles, Cancelable cancelable,
             const scoped_refptr<Window>& view,
             const MouseEventInit& init_dict);

  // Creates an event with its "initialized flag" unset.
  explicit MouseEvent(UninitializedFlag uninitialized_flag);

  void InitMouseEvent(const std::string& type, bool bubbles, bool cancelable,
                      const scoped_refptr<Window>& view, int32 detail,
                      int32 screen_x, int32 screen_y, int32 client_x,
                      int32 client_y, bool ctrl_key, bool alt_key,
                      bool shift_key, bool meta_key, uint16 button,
                      const scoped_refptr<EventTarget>& related_target);

  void InitMouseEvent(const std::string& type, bool bubbles, bool cancelable,
                      const scoped_refptr<Window>& view, int32 detail,
                      int32 screen_x, int32 screen_y, int32 client_x,
                      int32 client_y, const std::string& modifierslist,
                      uint16 button,
                      const scoped_refptr<EventTarget>& related_target);

  float screen_x() const { return screen_x_; }
  float screen_y() const { return screen_y_; }
  float client_x() const { return client_x_; }
  float client_y() const { return client_y_; }

  // Web API: CSSOM View Module: Extensions to the MouseEvent Interface
  // (partial interface). This also changes screen_* and client_* above from
  // long to double.
  //   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#extensions-to-the-mouseevent-interface
  float page_x() const;
  float page_y() const;
  float x() const { return static_cast<float>(client_x_); }
  float y() const { return static_cast<float>(client_y_); }
  float offset_x();
  float offset_y() const;

  int16_t button() const { return button_; }
  uint16_t buttons() const { return buttons_; }

  void set_related_target(const scoped_refptr<EventTarget>& target) {
    related_target_ = target;
  }

  const scoped_refptr<EventTarget>& related_target() const {
    return related_target_;
  }

  // Web API: UIEvent
  // This contains a value equal to the value stored in button+1.
  //   https://w3c.github.io/uievents/#dom-uievent-which
  uint32 which() const override;

  DEFINE_WRAPPABLE_TYPE(MouseEvent);
  void TraceMembers(script::Tracer* tracer) override;

 protected:
  ~MouseEvent() override {}

 private:
  float screen_x_;
  float screen_y_;
  float client_x_;
  float client_y_;
  int16_t button_;
  uint16_t buttons_;

  scoped_refptr<EventTarget> related_target_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MOUSE_EVENT_H_
