// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_FEATURE_POD_BUTTON_H_
#define ASH_SYSTEM_UNIFIED_FEATURE_POD_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/style/icon_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// TODO(crbug.com/40808951): Remove FeaturePodIconButton after the migration.
// A toggle button with an icon used by feature pods and in other places.
class ASH_EXPORT FeaturePodIconButton : public IconButton {
  METADATA_HEADER(FeaturePodIconButton, IconButton)

 public:
  FeaturePodIconButton(PressedCallback callback, bool is_togglable);
  FeaturePodIconButton(const FeaturePodIconButton&) = delete;
  FeaturePodIconButton& operator=(const FeaturePodIconButton&) = delete;
  ~FeaturePodIconButton() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_FEATURE_POD_BUTTON_H_
