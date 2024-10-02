// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_COLUMN_SPANNER_PATH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_COLUMN_SPANNER_PATH_H_

#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"

namespace blink {

// A path from the multicol container and down to a column spanner, each
// container represented as a step on the path. The outermost node is the
// multicol container, and the innermost one is the spanner itself. It is
// generated during the initial layout (column balancing) pass, and then sent
// into the layout algorithms in the next pass(es), so that we can tell whether
// a node is on the path between the multicol container and the spanner.
class NGColumnSpannerPath : public GarbageCollected<NGColumnSpannerPath> {
 public:
  explicit NGColumnSpannerPath(NGBlockNode block,
                               const NGColumnSpannerPath* child = nullptr)
      : box_(block.GetLayoutBox()), child_(child) {}

  NGBlockNode BlockNode() const { return NGBlockNode(box_); }
  const NGColumnSpannerPath* Child() const { return child_; }

  void Trace(Visitor* visitor) const {
    visitor->Trace(box_);
    visitor->Trace(child_);
  }

 private:
  Member<LayoutBox> box_;
  Member<const NGColumnSpannerPath> child_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_COLUMN_SPANNER_PATH_H_
