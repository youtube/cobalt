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

#ifndef COBALT_LAYOUT_LAYOUT_BOXES_H_
#define COBALT_LAYOUT_LAYOUT_BOXES_H_

#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/dom/dom_rect_list.h"
#include "cobalt/dom/layout_boxes.h"
#include "cobalt/layout/box.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace layout {

class LayoutBoxes : public dom::LayoutBoxes {
 public:
  LayoutBoxes() {}
  explicit LayoutBoxes(Boxes&& boxes) : boxes_(std::move(boxes)) {}
  ~LayoutBoxes() override{};

  // From: dom:LayoutBoxes
  //
  Type type() const override;

  scoped_refptr<dom::DOMRectList> GetClientRects() const override;
  bool IsInline() const override;

  float GetBorderEdgeLeft() const override;
  float GetBorderEdgeTop() const override;
  float GetBorderEdgeWidth() const override;
  float GetBorderEdgeHeight() const override;

  float GetBorderLeftWidth() const override;
  float GetBorderTopWidth() const override;

  float GetMarginEdgeWidth() const override;
  float GetMarginEdgeHeight() const override;

  math::Vector2dF GetPaddingEdgeOffset() const override;
  float GetPaddingEdgeWidth() const override;
  float GetPaddingEdgeHeight() const override;

  math::RectF GetScrollArea(dom::Directionality dir) const override;

  void InvalidateSizes() override;
  void InvalidateCrossReferences() override;
  void InvalidateRenderTreeNodes() override;
  void SetUiNavItem(const scoped_refptr<ui_navigation::NavItem>& item) override;

  // Other
  //
  const Boxes& boxes() { return boxes_; }

  base::Optional<std::pair<dom::Directionality, math::RectF>>&
  scroll_area_cache() override {
    return scroll_area_cache_;
  }

 private:
  // Returns the bounding rectangle of the border edges of the boxes.
  math::RectF GetBoundingBorderRectangle() const;

  void GetClientRectBoxes(const Boxes& boxes, Boxes* client_rect_boxes) const;

  Boxes boxes_;

  mutable base::Optional<std::pair<dom::Directionality, math::RectF>>
      scroll_area_cache_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_LAYOUT_BOXES_H_
