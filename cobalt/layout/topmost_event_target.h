// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_LAYOUT_TOPMOST_EVENT_TARGET_H_
#define COBALT_LAYOUT_TOPMOST_EVENT_TARGET_H_

#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/pointer_event.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/layout_boxes.h"
#include "cobalt/math/vector2d.h"
#include "cobalt/math/vector2d_f.h"
#include "cobalt/ui_navigation/scroll_engine/scroll_engine.h"
#include "cobalt/web/event.h"

namespace cobalt {
namespace layout {

class TopmostEventTarget {
 public:
  explicit TopmostEventTarget(
      ui_navigation::scroll_engine::ScrollEngine* scroll_engine);

  void MaybeSendPointerEvents(const scoped_refptr<web::Event>& event);

 private:
  scoped_refptr<dom::HTMLElement> FindTopmostEventTarget(
      const scoped_refptr<dom::Document>& document,
      const math::Vector2dF& coordinate);

  void HandleScrollState(scoped_refptr<dom::HTMLElement> target_element,
                         const dom::PointerEvent* pointer_event,
                         dom::PointerState* pointer_state,
                         dom::PointerEventInit* event_init);
  void CancelScrollsInParentNavItems(
      scoped_refptr<dom::HTMLElement> target_element);

  void ConsiderElement(dom::Element* element, const math::Vector2dF& coordinate,
                       bool consider_only_fixed_elements);
  void ConsiderBoxes(const scoped_refptr<dom::HTMLElement>& html_element,
                     LayoutBoxes* layout_boxes,
                     const math::Vector2dF& coordinate);

  base::WeakPtr<dom::HTMLElement> previous_html_element_weak_;
  scoped_refptr<dom::HTMLElement> html_element_;
  scoped_refptr<Box> box_;
  Box::RenderSequence render_sequence_;

  ui_navigation::scroll_engine::ScrollEngine* scroll_engine_;

  // This map stores the pointer types for which the 'prevent mouse event' flag
  // has been set as part of the compatibility mapping steps defined at
  // https://www.w3.org/TR/pointerevents/#compatibility-mapping-with-mouse-events.
  std::set<std::string> mouse_event_prevent_flags_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_TOPMOST_EVENT_TARGET_H_
