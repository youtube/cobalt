// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_pod_button.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

FeaturePodIconButton::FeaturePodIconButton(PressedCallback callback,
                                           bool is_togglable)
    : IconButton(std::move(callback),
                 IconButton::Type::kLarge,
                 /*icon=*/nullptr,
                 is_togglable,
                 /*has_border=*/true) {
  SetFlipCanvasOnPaintForRTLUI(false);
  GetViewAccessibility().SetIsLeaf(true);
}

FeaturePodIconButton::~FeaturePodIconButton() = default;

BEGIN_METADATA(FeaturePodIconButton)
END_METADATA

}  // namespace ash
