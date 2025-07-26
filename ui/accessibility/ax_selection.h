// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_SELECTION_H_
#define UI_ACCESSIBILITY_AX_SELECTION_H_

#include "ui/accessibility/ax_constants.mojom.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ui {

class AXTree;

// A data structure that can store either the selected range of nodes in the
// accessibility tree, or the location of the caret in the case of a
// "collapsed" selection.
class AX_EXPORT AXSelection final {
 public:
  AXSelection();
  explicit AXSelection(const AXTree&);
  AXSelection(const AXSelection&);
  ~AXSelection();

  // Returns true if this instance represents the position of the caret.
  constexpr bool IsCollapsed() const {
    return focus_object_id != kInvalidAXNodeID &&
           anchor_object_id == focus_object_id && anchor_offset == focus_offset;
  }

  bool is_backward = false;
  AXNodeID anchor_object_id = kInvalidAXNodeID;
  int anchor_offset = ax::mojom::kNoSelectionOffset;
  ax::mojom::TextAffinity anchor_affinity;
  AXNodeID focus_object_id = kInvalidAXNodeID;
  int focus_offset = ax::mojom::kNoSelectionOffset;
  ax::mojom::TextAffinity focus_affinity;

  // Returns a selection that does not use ignored nodes for endpoints.
  // If non_text_endpoints is true, returns an unignored selection but the
  // endpoints are adjusted so that they never fall on text objects, but are
  // moved to the text nodes' parents instead.
  AXSelection& ToUnignoredSelection(bool non_text_endpoints = false);

 private:
  AXTreeID tree_id_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_SELECTION_H_
