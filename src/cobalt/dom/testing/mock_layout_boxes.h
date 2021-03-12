// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_TESTING_MOCK_LAYOUT_BOXES_H_
#define COBALT_DOM_TESTING_MOCK_LAYOUT_BOXES_H_

#include "cobalt/dom/layout_boxes.h"

namespace cobalt {
namespace dom {
namespace testing {

class MockLayoutBoxes : public LayoutBoxes {
 public:
  MOCK_CONST_METHOD0(type, Type());
  MOCK_CONST_METHOD0(GetClientRects, scoped_refptr<DOMRectList>());

  MOCK_CONST_METHOD0(IsInline, bool());

  MOCK_CONST_METHOD0(GetBorderEdgeLeft, float());
  MOCK_CONST_METHOD0(GetBorderEdgeTop, float());
  MOCK_CONST_METHOD0(GetBorderEdgeWidth, float());
  MOCK_CONST_METHOD0(GetBorderEdgeHeight, float());
  MOCK_CONST_METHOD0(GetBorderEdgeOffsetFromContainingBlock, math::Vector2dF());

  MOCK_CONST_METHOD0(GetBorderLeftWidth, float());
  MOCK_CONST_METHOD0(GetBorderTopWidth, float());

  MOCK_CONST_METHOD0(GetMarginEdgeWidth, float());
  MOCK_CONST_METHOD0(GetMarginEdgeHeight, float());

  MOCK_CONST_METHOD0(GetPaddingEdgeOffset, math::Vector2dF());
  MOCK_CONST_METHOD0(GetPaddingEdgeWidth, float());
  MOCK_CONST_METHOD0(GetPaddingEdgeHeight, float());
  MOCK_CONST_METHOD0(GetPaddingEdgeOffsetFromContainingBlock,
                     math::Vector2dF());

  MOCK_CONST_METHOD0(GetContentEdgeOffset, math::Vector2dF());
  MOCK_CONST_METHOD0(GetContentEdgeWidth, float());
  MOCK_CONST_METHOD0(GetContentEdgeHeight, float());
  MOCK_CONST_METHOD0(GetContentEdgeOffsetFromContainingBlock,
                     math::Vector2dF());

  MOCK_CONST_METHOD1(GetScrollArea, math::RectF(dom::Directionality));

  MOCK_METHOD0(InvalidateSizes, void());
  MOCK_METHOD0(InvalidateCrossReferences, void());
  MOCK_METHOD0(InvalidateRenderTreeNodes, void());
  MOCK_METHOD1(SetUiNavItem,
               void(const scoped_refptr<ui_navigation::NavItem>& item));
};
}  // namespace testing
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TESTING_MOCK_LAYOUT_BOXES_H_
