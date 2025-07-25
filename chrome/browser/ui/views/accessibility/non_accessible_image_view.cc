// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"

NonAccessibleImageView::NonAccessibleImageView() {}

NonAccessibleImageView::~NonAccessibleImageView() {}

void NonAccessibleImageView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->AddState(ax::mojom::State::kInvisible);
}

BEGIN_METADATA(NonAccessibleImageView, views::ImageView)
END_METADATA
