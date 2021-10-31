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

#include "cobalt/layout/layout_boxes.h"

#include "cobalt/cssom/computed_style_utils.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/layout/anonymous_block_box.h"
#include "cobalt/layout/container_box.h"
#include "cobalt/layout/rect_layout_unit.h"
#include "cobalt/layout/size_layout_unit.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

LayoutBoxes::Type LayoutBoxes::type() const { return kLayoutLayoutBoxes; }

// Algorithm for GetClientRects:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-element-getclientrects
scoped_refptr<dom::DOMRectList> LayoutBoxes::GetClientRects() const {
  // 1. If the element on which it was invoked does not have an associated
  // layout box return an empty DOMRectList object and stop this algorithm.

  // 2. If the element has an associated SVG layout box return a DOMRectList
  // object containing a single DOMRect object that describes the bounding box
  // of the element as defined by the SVG specification, applying the transforms
  // that apply to the element and its ancestors.

  // 3. Return a DOMRectList object containing a list of DOMRect objects in
  // content order describing the bounding border boxes (including those with a
  // height or width of zero) with the following constraints:
  //  . Apply the transforms that apply to the element and its ancestors.
  //  . If the element on which the method was invoked has a computed value for
  //    the 'display' property of 'table' or 'inline-table' include both the
  //    table box and the caption box, if any, but not the anonymous container
  //    box.
  //  . Replace each anonymous block box with its child box(es) and repeat this
  //    until no anonymous block boxes are left in the final list.

  Boxes client_rect_boxes;
  GetClientRectBoxes(boxes_, &client_rect_boxes);

  scoped_refptr<dom::DOMRectList> dom_rect_list(new dom::DOMRectList());
  for (Boxes::const_iterator box_iterator = client_rect_boxes.begin();
       box_iterator != client_rect_boxes.end(); ++box_iterator) {
    RectLayoutUnit transformed_border_box(
        (*box_iterator)
            ->GetTransformedBoxFromRootWithScroll(
                (*box_iterator)->GetBorderBoxFromMarginBox()));
    dom_rect_list->AppendDOMRect(
        new dom::DOMRect(transformed_border_box.x().toFloat(),
                         transformed_border_box.y().toFloat(),
                         transformed_border_box.width().toFloat(),
                         transformed_border_box.height().toFloat()));
  }

  return dom_rect_list;
}

bool LayoutBoxes::IsInline() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->computed_style()->display() ==
         cssom::KeywordValue::GetInline();
}

float LayoutBoxes::GetBorderEdgeLeft() const {
  return GetBoundingBorderRectangle().x();
}

float LayoutBoxes::GetBorderEdgeTop() const {
  return GetBoundingBorderRectangle().y();
}

float LayoutBoxes::GetBorderEdgeWidth() const {
  return GetBoundingBorderRectangle().width();
}

float LayoutBoxes::GetBorderEdgeHeight() const {
  return GetBoundingBorderRectangle().height();
}

float LayoutBoxes::GetBorderLeftWidth() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->border_left_width().toFloat();
}

float LayoutBoxes::GetBorderTopWidth() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->border_top_width().toFloat();
}

float LayoutBoxes::GetMarginEdgeWidth() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->GetMarginBoxWidth().toFloat();
}

float LayoutBoxes::GetMarginEdgeHeight() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->GetMarginBoxHeight().toFloat();
}

math::Vector2dF LayoutBoxes::GetPaddingEdgeOffset() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->GetPaddingBoxOffsetFromRoot(
      false /*transform_forms_root*/);
}

float LayoutBoxes::GetPaddingEdgeWidth() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->GetPaddingBoxWidth().toFloat();
}

float LayoutBoxes::GetPaddingEdgeHeight() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->GetPaddingBoxHeight().toFloat();
}

math::RectF LayoutBoxes::GetScrollArea(dom::Directionality dir) const {
  // https://www.w3.org/TR/cssom-view-1/#scrolling-area
  // For rightward and downward:
  //   Top edge: The element's top padding edge.
  //   Right edge: The right-most edge of the element's right padding edge and
  //     the right margin edge of all of the element's descendants' boxes,
  //     excluding boxes that have an ancestor of the element as their
  //     containing block.
  //   Bottom edge: The bottom-most edge of the element's bottom padding edge
  //     and the bottom margin edge of all of the element's descendants' boxes,
  //     excluding boxes that have an ancestor of the element as their
  //     containing block.
  //   Left edge: The element's left padding edge.
  // See also https://www.w3.org/TR/css-overflow-3/#scrollable
  if (boxes_.size() == 0) {
    return math::RectF();
  }

  // Return the cached results if applicable.
  if (scroll_area_cache_ && scroll_area_cache_->first == dir) {
    return scroll_area_cache_->second;
  }

  // Calculate the scroll area. It should be relative to these layout boxes --
  // not to the root.
  math::RectF padding_area;
  math::RectF scroll_area;

  for (scoped_refptr<Box> layout_box : boxes_) {
    // Include the box's own content and padding areas.
    SizeLayoutUnit padding_size = layout_box->GetClampedPaddingBoxSize();
    padding_area.Union(math::RectF(0, 0, padding_size.width().toFloat(),
                                   padding_size.height().toFloat()));
    const ContainerBox* container_box = layout_box->AsContainerBox();
    if (!container_box) {
      continue;
    }

    std::vector<const Boxes*> child_boxes_list;
    child_boxes_list.push_back(&container_box->child_boxes());

    while (!child_boxes_list.empty()) {
      // Process the next set of child boxes.
      const Boxes* child_boxes = child_boxes_list.back();
      child_boxes_list.pop_back();

      for (const scoped_refptr<Box>& box : *child_boxes) {
        // Exclude boxes that have an ancestor of |container_box| as their
        // containing block.
        for (const ContainerBox* container = box->GetContainingBlock();
             container; container = container->GetContainingBlock()) {
          if (container == container_box) {
            // Add this box's border box to the scroll area.
            RectLayoutUnit border_box =
                box->GetTransformedBoxFromContainingBlock(
                    container_box, box->GetBorderBoxFromMarginBox());
            scroll_area.Union(math::RectF(
                border_box.x().toFloat(), border_box.y().toFloat(),
                border_box.width().toFloat(), border_box.height().toFloat()));

            // Include the scrollable overflow regions of the contents provided
            // they are visible (i.e. container has overflow: visible).
            if (box->AsContainerBox() &&
                !IsOverflowCropped(box->computed_style())) {
              child_boxes_list.push_back(&box->AsContainerBox()->child_boxes());
            }
            break;
          }
        }
      }
    }
  }

  scroll_area.Union(padding_area);

  // Clip the scroll area according to directionality.
  float left = scroll_area.x();
  float right = scroll_area.right();
  float top = padding_area.y();
  float bottom = scroll_area.bottom();
  switch (dir) {
    case dom::kLeftToRightDirectionality:
      left = padding_area.x();
      break;
    case dom::kRightToLeftDirectionality:
      right = padding_area.right();
      break;
  }

  // Cache the results to speed up future queries.
  scroll_area_cache_.emplace(
      dir, math::RectF(left, top, right - left, bottom - top));
  return scroll_area_cache_->second;
}

void LayoutBoxes::InvalidateSizes() {
  for (Boxes::const_iterator box_iterator = boxes_.begin();
       box_iterator != boxes_.end(); ++box_iterator) {
    Box* box = *box_iterator;
    do {
      box->InvalidateUpdateSizeInputsOfBoxAndAncestors();
      box = box->GetSplitSibling();
    } while (box != NULL);
  }
  scroll_area_cache_.reset();
}

void LayoutBoxes::InvalidateCrossReferences() {
  for (Boxes::const_iterator box_iterator = boxes_.begin();
       box_iterator != boxes_.end(); ++box_iterator) {
    Box* box = *box_iterator;
    do {
      box->InvalidateCrossReferencesOfBoxAndAncestors();
      box = box->GetSplitSibling();
    } while (box != NULL);
  }
  scroll_area_cache_.reset();
}

void LayoutBoxes::InvalidateRenderTreeNodes() {
  for (Boxes::const_iterator box_iterator = boxes_.begin();
       box_iterator != boxes_.end(); ++box_iterator) {
    Box* box = *box_iterator;
    do {
      box->InvalidateRenderTreeNodesOfBoxAndAncestors();
      box = box->GetSplitSibling();
    } while (box != NULL);
  }
}

void LayoutBoxes::SetUiNavItem(
    const scoped_refptr<ui_navigation::NavItem>& item) {
  for (Boxes::const_iterator box_iterator = boxes_.begin();
       box_iterator != boxes_.end(); ++box_iterator) {
    Box* box = *box_iterator;
    box->SetUiNavItem(item);
  }
}

math::RectF LayoutBoxes::GetBoundingBorderRectangle() const {
  // In the CSSOM View extensions to the HTMLElement interface, at
  // https://www.w3.org/TR/2013/WD-cssom-view-20131217/#extensions-to-the-htmlelement-interface,
  // the standard mentions the 'first CSS layout box associated with the
  // element' and links to a definition 'The term CSS layout box refers to the
  // same term in CSS', which is followed by a note 'ISSUE 2' that mentions 'The
  // terms CSS layout box and SVG layout box are not currently defined by CSS or
  // SVG', at https://www.w3.org/TR/2013/WD-cssom-view-20131217/#css-layout-box.
  // This function calculates the bounding box of the border boxes of the layout
  // boxes, mirroring behavior of most other browsers for the 'first CSS layout
  // box associated with the element'.
  RectLayoutUnit bounding_rectangle;

  for (Boxes::const_iterator box_iterator = boxes_.begin();
       box_iterator != boxes_.end(); ++box_iterator) {
    Box* box = *box_iterator;
    do {
      bounding_rectangle.Union(
          box->GetBorderBoxFromRoot(false /*transform_forms_root*/));
      box = box->GetSplitSibling();
    } while (box != NULL);
  }

  return math::RectF(bounding_rectangle.x().toFloat(),
                     bounding_rectangle.y().toFloat(),
                     bounding_rectangle.width().toFloat(),
                     bounding_rectangle.height().toFloat());
}

void LayoutBoxes::GetClientRectBoxes(const Boxes& boxes,
                                     Boxes* client_rect_boxes) const {
  for (Boxes::const_iterator box_iterator = boxes.begin();
       box_iterator != boxes.end(); ++box_iterator) {
    Box* box = *box_iterator;
    do {
      // Replace each anonymous block box with its child box(es) and repeat this
      // until no anonymous block boxes are left in the final list.
      const AnonymousBlockBox* anonymous_block_box = box->AsAnonymousBlockBox();
      if (anonymous_block_box) {
        GetClientRectBoxes(anonymous_block_box->child_boxes(),
                           client_rect_boxes);
      } else if (!box->AsTextBox()) {
        // Only add the box if it isn't a text box. Text boxes are anonymous
        // inline boxes and shouldn't be included.
        client_rect_boxes->push_back(box);
      }

      box = box->GetSplitSibling();
    } while (box != NULL);
  }
}

}  // namespace layout
}  // namespace cobalt
