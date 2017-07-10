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

#ifndef COBALT_LAYOUT_TOPMOST_EVENT_TARGET_H_
#define COBALT_LAYOUT_TOPMOST_EVENT_TARGET_H_

#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/window.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/layout_boxes.h"
#include "cobalt/math/vector2d.h"
#include "cobalt/math/vector2d_f.h"

namespace cobalt {
namespace layout {

class TopmostEventTarget {
 public:
  explicit TopmostEventTarget() {}

  void MaybeSendPointerEvents(const scoped_refptr<dom::Event>& event,
                              const scoped_refptr<dom::Window>& window);

  scoped_refptr<dom::HTMLElement> previous_html_element_;
  scoped_refptr<dom::HTMLElement> html_element_;
  scoped_refptr<Box> box_;
  Box::RenderSequence render_sequence_;

 private:
  void FindTopmostEventTarget(const scoped_refptr<dom::Document>& document,
                              const math::Vector2dF& coordinate);

  void ConsiderElement(const scoped_refptr<dom::HTMLElement>& html_element,
                       const math::Vector2dF& coordinate);
  void ConsiderBoxes(const scoped_refptr<dom::HTMLElement>& html_element,
                     LayoutBoxes* layout_boxes,
                     const math::Vector2dF& coordinate);
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_TOPMOST_EVENT_TARGET_H_
