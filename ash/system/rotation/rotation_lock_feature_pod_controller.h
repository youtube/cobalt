// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ROTATION_ROTATION_LOCK_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_ROTATION_ROTATION_LOCK_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class FeaturePodButton;
class FeatureTile;

// Controller of a feature pod button that toggles rotation lock mode.
// Pre-QsRevamp the button is toggled when rotation is locked.
// Post-QsRevamp the tile is toggled when rotation is unlocked (i.e. auto-rotate
// is enabled).
class ASH_EXPORT RotationLockFeaturePodController
    : public FeaturePodControllerBase,
      public ScreenOrientationController::Observer {
 public:
  RotationLockFeaturePodController();

  RotationLockFeaturePodController(const RotationLockFeaturePodController&) =
      delete;
  RotationLockFeaturePodController& operator=(
      const RotationLockFeaturePodController&) = delete;

  ~RotationLockFeaturePodController() override;

  // Referenced by `UnifiedSystemTrayController` to know whether to construct a
  // Primary or Compact tile.
  static bool CalculateButtonVisibility();

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;

  // ScreenOrientationController::Observer:
  void OnUserRotationLockChanged() override;

 private:
  void UpdateButton();
  void UpdateTile();

  // Owned by views hierarchy.
  raw_ptr<FeaturePodButton, ExperimentalAsh> button_ = nullptr;
  raw_ptr<FeatureTile, ExperimentalAsh> tile_ = nullptr;

  base::WeakPtrFactory<RotationLockFeaturePodController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_ROTATION_ROTATION_LOCK_FEATURE_POD_CONTROLLER_H_
