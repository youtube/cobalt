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

#ifndef COBALT_LAYOUT_LAYOUT_BOXES_H_
#define COBALT_LAYOUT_LAYOUT_BOXES_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/dom_rect_list.h"
#include "cobalt/dom/layout_boxes.h"
#include "cobalt/layout/box.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace layout {

class LayoutBoxes : public dom::LayoutBoxes {
 public:
  LayoutBoxes();
  ~LayoutBoxes() OVERRIDE;

  // From: dom:LayoutBoxes
  //
  Type type() const OVERRIDE;

  scoped_refptr<dom::DOMRectList> GetClientRects() const OVERRIDE;
  bool IsInlineLevel() const OVERRIDE;

  float GetBorderEdgeLeft() const OVERRIDE;
  float GetBorderEdgeTop() const OVERRIDE;
  float GetBorderEdgeWidth() const OVERRIDE;
  float GetBorderEdgeHeight() const OVERRIDE;

  float GetBorderLeftWidth() const OVERRIDE;
  float GetBorderTopWidth() const OVERRIDE;

  float GetMarginEdgeWidth() const OVERRIDE;
  float GetMarginEdgeHeight() const OVERRIDE;

  float GetPaddingEdgeLeft() const OVERRIDE;
  float GetPaddingEdgeTop() const OVERRIDE;
  float GetPaddingEdgeWidth() const OVERRIDE;
  float GetPaddingEdgeHeight() const OVERRIDE;

  void InvalidateSizes() OVERRIDE;
  void InvalidateCrossReferences() OVERRIDE;

  // Other
  //
  void SwapBoxes(Boxes& boxes) { boxes_.swap(boxes); }
  const Boxes& boxes() { return boxes_; }

 private:
  // Returns the bounding rectangle of the border edges of the boxes.
  math::RectF GetBoundingBorderRectangle() const;

  Boxes boxes_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_LAYOUT_BOXES_H_
