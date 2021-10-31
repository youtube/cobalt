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

#ifndef COBALT_DOM_LAYOUT_BOXES_H_
#define COBALT_DOM_LAYOUT_BOXES_H_

#include "base/memory/ref_counted.h"
#include "cobalt/dom/directionality.h"
#include "cobalt/dom/dom_rect_list.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/math/vector2d_f.h"
#include "cobalt/ui_navigation/nav_item.h"

namespace cobalt {
namespace dom {

// This class gives the DOM an interface to the boxes generated from the element
// during layout. This allows incremental box generation by letting the DOM tree
// invalidate the boxes when an element changes.

class LayoutBoxes {
 public:
  virtual ~LayoutBoxes() {}

  // All classes implementing this interface have a unique Type value.
  enum Type {
    kLayoutLayoutBoxes,
  };

  // Returns the type of layout boxes.
  virtual Type type() const = 0;

  // Methods to support the CSSOM View Module extensions to the Element and
  // HTMLElement Interfaces. See
  // https://www.w3.org/TR/2013/WD-cssom-view-20131217/#extensions-to-the-element-interface
  // and
  // https://www.w3.org/TR/2013/WD-cssom-view-20131217/#extensions-to-the-htmlelement-interface.

  // Returns a DOMRectList object containing a list of DOMRect objects in
  // content order describing the bounding border boxes.
  virtual scoped_refptr<dom::DOMRectList> GetClientRects() const = 0;

  // Returns true if the container box level is inline.
  virtual bool IsInline() const = 0;

  // Returns border edge values.
  // See https://www.w3.org/TR/CSS21/box.html#box-dimensions
  virtual float GetBorderEdgeLeft() const = 0;
  virtual float GetBorderEdgeTop() const = 0;
  virtual float GetBorderEdgeWidth() const = 0;
  virtual float GetBorderEdgeHeight() const = 0;

  // Returns the border width (thickness) values.
  virtual float GetBorderLeftWidth() const = 0;
  virtual float GetBorderTopWidth() const = 0;

  // Returns margin edge values.
  // See https://www.w3.org/TR/CSS21/box.html#box-dimensions
  virtual float GetMarginEdgeWidth() const = 0;
  virtual float GetMarginEdgeHeight() const = 0;

  // Returns padding edge values.
  // See https://www.w3.org/TR/CSS21/box.html#box-dimensions
  virtual math::Vector2dF GetPaddingEdgeOffset() const = 0;
  virtual float GetPaddingEdgeWidth() const = 0;
  virtual float GetPaddingEdgeHeight() const = 0;

  // Return scrolling area.
  // See https://www.w3.org/TR/cssom-view-1/#scrolling-area
  virtual math::RectF GetScrollArea(Directionality dir) const = 0;

  // Invalidate the sizes of the layout boxes so that they'll be recalculated
  // during the next layout.
  virtual void InvalidateSizes() = 0;
  // Invalidate the cross references, which relates to both positioned children
  // of containing blocks and z-index children of stacking contexts.
  virtual void InvalidateCrossReferences() = 0;
  // Invalidate the layout box's render tree nodes.
  virtual void InvalidateRenderTreeNodes() = 0;

  // Update the navigation item associated with the layout boxes.
  virtual void SetUiNavItem(
      const scoped_refptr<ui_navigation::NavItem>& item) = 0;

 protected:
  LayoutBoxes() {}
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_LAYOUT_BOXES_H_
