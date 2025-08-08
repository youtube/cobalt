// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/test_list_grid_view.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"

namespace views {
namespace test {

TestListGridView::TestListGridView() = default;
TestListGridView::~TestListGridView() = default;

void TestListGridView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kListGrid;
  if (aria_row_count) {
    node_data->AddIntAttribute(ax::mojom::IntAttribute::kAriaRowCount,
                               *aria_row_count);
  }
  if (aria_column_count) {
    node_data->AddIntAttribute(ax::mojom::IntAttribute::kAriaColumnCount,
                               *aria_column_count);
  }
  if (table_row_count) {
    node_data->AddIntAttribute(ax::mojom::IntAttribute::kTableRowCount,
                               *table_row_count);
  }
  if (table_column_count) {
    node_data->AddIntAttribute(ax::mojom::IntAttribute::kTableColumnCount,
                               *table_column_count);
  }
}

void TestListGridView::SetAriaTableSize(int row_count, int column_count) {
  aria_row_count = absl::make_optional(row_count);
  aria_column_count = absl::make_optional(column_count);
}

void TestListGridView::SetTableSize(int row_count, int column_count) {
  table_row_count = absl::make_optional(row_count);
  table_column_count = absl::make_optional(column_count);
}

void TestListGridView::UnsetAriaTableSize() {
  aria_row_count = absl::nullopt;
  aria_column_count = absl::nullopt;
}

void TestListGridView::UnsetTableSize() {
  table_row_count = absl::nullopt;
  table_column_count = absl::nullopt;
}

}  // namespace test
}  // namespace views
