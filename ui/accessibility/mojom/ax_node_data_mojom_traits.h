// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_MOJOM_AX_NODE_DATA_MOJOM_TRAITS_H_
#define UI_ACCESSIBILITY_MOJOM_AX_NODE_DATA_MOJOM_TRAITS_H_

#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_relative_bounds.h"
#include "ui/accessibility/mojom/ax_node_data.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<ax::mojom::AXNodeDataDataView, ui::AXNodeData> {
  static int32_t id(const ui::AXNodeData& p) { return p.id; }
  static ax::mojom::Role role(const ui::AXNodeData& p) { return p.role; }
  static uint32_t state(const ui::AXNodeData& p) { return p.state; }
  static uint64_t actions(const ui::AXNodeData& p) { return p.actions; }
  static const std::vector<std::pair<ax::mojom::StringAttribute, std::string>>&
  string_attributes(const ui::AXNodeData& p) {
    return p.string_attributes;
  }
  static const std::vector<std::pair<ax::mojom::IntAttribute, int32_t>>&
  int_attributes(const ui::AXNodeData& p) {
    return p.int_attributes;
  }
  static const std::vector<std::pair<ax::mojom::FloatAttribute, float>>&
  float_attributes(const ui::AXNodeData& p) {
    return p.float_attributes;
  }
  static const std::vector<std::pair<ax::mojom::BoolAttribute, bool>>&
  bool_attributes(const ui::AXNodeData& p) {
    return p.bool_attributes;
  }
  static const std::vector<
      std::pair<ax::mojom::IntListAttribute, std::vector<int32_t>>>&
  intlist_attributes(const ui::AXNodeData& p) {
    return p.intlist_attributes;
  }
  static const std::vector<
      std::pair<ax::mojom::StringListAttribute, std::vector<std::string>>>&
  stringlist_attributes(const ui::AXNodeData& p) {
    return p.stringlist_attributes;
  }
  static const std::vector<std::pair<std::string, std::string>>&
  html_attributes(const ui::AXNodeData& p) {
    return p.html_attributes;
  }
  static const std::vector<int32_t>& child_ids(const ui::AXNodeData& p) {
    return p.child_ids;
  }
  static ui::AXRelativeBounds relative_bounds(const ui::AXNodeData& p) {
    return p.relative_bounds;
  }
  static bool Read(ax::mojom::AXNodeDataDataView data, ui::AXNodeData* out);
};

}  // namespace mojo

#endif  // UI_ACCESSIBILITY_MOJOM_AX_NODE_DATA_MOJOM_TRAITS_H_
