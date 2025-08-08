// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_ACTIVE_POPUP_H_
#define UI_ACCESSIBILITY_AX_ACTIVE_POPUP_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node_data.h"

namespace ui {

AX_EXPORT absl::optional<AXNodeID> GetActivePopupAxUniqueId();

AX_EXPORT void SetActivePopupAxUniqueId(absl::optional<AXNodeID> ax_unique_id);

AX_EXPORT void ClearActivePopupAxUniqueId();

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_ACTIVE_POPUP_H_
