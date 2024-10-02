// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/arc_accessibility_test_util.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace {
void AddAction(
    std::vector<arc::mojom::AccessibilityActionInAndroidPtr>* actions,
    int id,
    absl::optional<std::string> label) {
  actions->push_back(arc::mojom::AccessibilityActionInAndroid::New());
  arc::mojom::AccessibilityActionInAndroid* action = actions->back().get();
  action->id = id;
  if (label) {
    action->label = label;
  }
}
}  // namespace

namespace arc {

void AddStandardAction(mojom::AccessibilityNodeInfoData* node,
                       mojom::AccessibilityActionType action_type,
                       absl::optional<std::string> label) {
  if (!node->standard_actions) {
    node->standard_actions =
        std::vector<mojom::AccessibilityActionInAndroidPtr>();
  }
  AddAction(&node->standard_actions.value(), static_cast<int>(action_type),
            label);
}

void AddCustomAction(mojom::AccessibilityNodeInfoData* node,
                     int id,
                     std::string label) {
  if (!node->custom_actions) {
    node->custom_actions =
        std::vector<mojom::AccessibilityActionInAndroidPtr>();
  }
  AddAction(&node->custom_actions.value(), id, label);
}

}  // namespace arc
