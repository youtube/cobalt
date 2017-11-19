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
#include "cobalt/dom/html_element.h"

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
      screen_x_(static_cast<float>(init_dict.screen_x())),
      screen_y_(static_cast<float>(init_dict.screen_y())),
      client_x_(static_cast<float>(init_dict.client_x())),
      client_y_(static_cast<float>(init_dict.client_y())),
      button_(init_dict.button()),
      buttons_(init_dict.buttons()),
      related_target_(init_dict.related_target()) {}

MouseEvent::MouseEvent(base::Token type, const scoped_refptr<Window>& view,
                       const MouseEventInit& init_dict)
    : UIEventWithKeyState(type, kBubbles, kCancelable, view, init_dict),
      screen_x_(static_cast<float>(init_dict.screen_x())),
      screen_y_(static_cast<float>(init_dict.screen_y())),
      client_x_(static_cast<float>(init_dict.client_x())),
      client_y_(static_cast<float>(init_dict.client_y())),
      button_(init_dict.button()),
      buttons_(init_dict.buttons()),
      related_target_(init_dict.related_target()) {}

MouseEvent::MouseEvent(base::Token type, Bubbles bubbles, Cancelable cancelable,
                       const scoped_refptr<Window>& view,
                       const MouseEventInit& init_dict)
    : UIEventWithKeyState(type, bubbles, cancelable, view, init_dict),
      screen_x_(static_cast<float>(init_dict.screen_x())),
      screen_y_(static_cast<float>(init_dict.screen_y())),
      client_x_(static_cast<float>(init_dict.client_x())),
      client_y_(static_cast<float>(init_dict.client_y())),
      button_(init_dict.button()),
      buttons_(init_dict.buttons()),
      related_target_(init_dict.related_target()) {}

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

float MouseEvent::page_x() const {
  // Algorithm for pageX
  //  https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-mouseevent-pagex

  // In Cobalt all steps are equivalent to returning clientX.
  return static_cast<float>(client_x());
}

float MouseEvent::page_y() const {
  // Algorithm for pageY
  //  https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-mouseevent-pagey

  // In Cobalt all steps are equivalent to returning clientY.
  return static_cast<float>(client_y());
}

float MouseEvent::offset_x() {
  // Algorithm for offsetX
  //  https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-mouseevent-offsetx

  // If the event's dispatch flag is set,
  if (IsBeingDispatched()) {
    // return the x-coordinate of the position where the event occurred relative
    // to the origin of the padding edge of the target node, ignoring the
    // transforms that apply to the element and its ancestors and terminate
    // these steps.
    float target_node_padding_edge = 0.0f;

    Node* node = base::polymorphic_downcast<Node*>(target().get());
    if (node && node->IsElement()) {
      scoped_refptr<HTMLElement> html_element =
          node->AsElement()->AsHTMLElement();
      if (html_element && html_element->layout_boxes()) {
        target_node_padding_edge =
            html_element->layout_boxes()->GetPaddingEdgeLeft();
      }
    }
    return client_x() - target_node_padding_edge;
  }

  // Return the value of the event's pageX attribute.
  return page_x();
}

float MouseEvent::offset_y() const {
  // Algorithm for offsetY
  //  https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-mouseevent-offsety

  // If the event's dispatch flag is set,
  if (IsBeingDispatched()) {
    // return the y-coordinate of the position where the event occurred relative
    // to the origin of the padding edge of the target node, ignoring the
    // transforms that apply to the element and its ancestors, and terminate
    // these steps.
    float target_node_padding_edge = 0.0f;

    Node* node = base::polymorphic_downcast<Node*>(target().get());
    if (node && node->IsElement()) {
      scoped_refptr<HTMLElement> html_element =
          node->AsElement()->AsHTMLElement();
      if (html_element && html_element->layout_boxes()) {
        target_node_padding_edge =
            html_element->layout_boxes()->GetPaddingEdgeTop();
      }
    }
    return client_y() - target_node_padding_edge;
  }

  // Return the value of the event's pageY attribute.
  return page_y();
}

uint32 MouseEvent::which() const { return button_ + 1; }

void MouseEvent::TraceMembers(script::Tracer* tracer) {
  UIEventWithKeyState::TraceMembers(tracer);

  tracer->Trace(related_target_);
}

}  // namespace dom
}  // namespace cobalt
