// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_TEST_AX_NODE_HELPER_H_
#define UI_ACCESSIBILITY_TEST_AX_NODE_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_clipping_behavior.h"
#include "ui/accessibility/ax_coordinate_system.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_offscreen_result.h"
#include "ui/accessibility/ax_tree.h"

namespace ui {

// For testing, a TestAXNodeHelper wraps an AXNode. This is a simple
// version of TestAXNodeWrapper.
class TestAXNodeHelper {
 public:
  // Create TestAXNodeHelper instances on-demand from an AXTree and AXNode.
  static TestAXNodeHelper* GetOrCreate(AXTree* tree, AXNode* node);
  ~TestAXNodeHelper();

  gfx::Rect GetBoundsRect(const AXCoordinateSystem coordinate_system,
                          const AXClippingBehavior clipping_behavior,
                          AXOffscreenResult* offscreen_result) const;
  gfx::Rect GetInnerTextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      const AXCoordinateSystem coordinate_system,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result) const;

 private:
  TestAXNodeHelper(AXTree* tree, AXNode* node);
  int InternalChildCount() const;
  TestAXNodeHelper* InternalGetChild(int index) const;
  const AXNodeData& GetData() const;
  gfx::RectF GetLocation() const;
  gfx::RectF GetInlineTextRect(const int start_offset,
                               const int end_offset) const;
  // Helper for determining if the two rects, including empty rects, intersect
  // each other.
  bool Intersects(gfx::RectF rect1, gfx::RectF rect2) const;

  // Determine the offscreen status of a particular element given its bounds.
  AXOffscreenResult DetermineOffscreenResult(gfx::RectF bounds) const;

  raw_ptr<AXTree> tree_;
  raw_ptr<AXNode> node_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_TEST_AX_NODE_HELPER_H_
