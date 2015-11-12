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

#include "cobalt/layout/layout_boxes.h"

#include "cobalt/layout/container_box.h"

namespace cobalt {
namespace layout {

LayoutBoxes::LayoutBoxes() {}

LayoutBoxes::~LayoutBoxes() {}

LayoutBoxes::Type LayoutBoxes::type() const { return kLayoutLayoutBoxes; }

scoped_refptr<dom::DOMRect> LayoutBoxes::GetBoundingClientRect() const {
  // TODO(***REMOVED***): Implement algorithm for GetBoundingClientRect().
  return make_scoped_refptr(new dom::DOMRect());
}

bool LayoutBoxes::IsInlineLevel() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->GetLevel() == Box::kInlineLevel;
}

float LayoutBoxes::GetBorderEdgeLeft() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->GetBorderBoxLeftEdge();
}

float LayoutBoxes::GetBorderEdgeTop() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->GetBorderBoxTopEdge();
}

float LayoutBoxes::GetBorderEdgeWidth() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->GetBorderBoxWidth();
}

float LayoutBoxes::GetBorderEdgeHeight() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->GetBorderBoxHeight();
}

float LayoutBoxes::GetBorderLeftWidth() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->border_left_width();
}

float LayoutBoxes::GetBorderTopWidth() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->border_top_width();
}

float LayoutBoxes::GetMarginEdgeWidth() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->GetMarginBoxWidth();
}

float LayoutBoxes::GetMarginEdgeHeight() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->GetMarginBoxHeight();
}

float LayoutBoxes::GetPaddingEdgeLeft() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->GetPaddingBoxLeftEdge();
}

float LayoutBoxes::GetPaddingEdgeTop() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->GetPaddingBoxTopEdge();
}

float LayoutBoxes::GetPaddingEdgeWidth() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->GetPaddingBoxWidth();
}

float LayoutBoxes::GetPaddingEdgeHeight() const {
  DCHECK(!boxes_.empty());
  return boxes_.front()->GetPaddingBoxHeight();
}

}  // namespace layout
}  // namespace cobalt
