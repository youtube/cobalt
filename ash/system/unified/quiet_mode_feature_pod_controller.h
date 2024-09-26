// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_QUIET_MODE_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_UNIFIED_QUIET_MODE_FEATURE_POD_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/notifier_settings_observer.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/message_center_observer.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of a feature pod button that toggles do-not-disturb mode.
// If the do-not-disturb mode is enabled, the button indicates it by bright
// background color and different icon.
class ASH_EXPORT QuietModeFeaturePodController
    : public FeaturePodControllerBase,
      public message_center::MessageCenterObserver,
      public NotifierSettingsObserver {
 public:
  explicit QuietModeFeaturePodController(
      UnifiedSystemTrayController* tray_controller);

  QuietModeFeaturePodController(const QuietModeFeaturePodController&) = delete;
  QuietModeFeaturePodController& operator=(
      const QuietModeFeaturePodController&) = delete;

  ~QuietModeFeaturePodController() override;

  // Referenced by `UnifiedSystemTrayController` to know whether to construct a
  // Primary or Compact tile.
  static bool CalculateButtonVisibility();

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;
  void OnLabelPressed() override;

  // message_center::MessageCenterObserver:
  void OnQuietModeChanged(bool in_quiet_mode) override;

  // NotifierSettingsObserver:
  void OnNotifiersUpdated(
      const std::vector<NotifierMetadata>& notifiers) override;

 private:
  std::u16string GetQuietModeStateTooltip();

  void RecordDisabledNotifierCount(int disabled_count);

  const raw_ptr<UnifiedSystemTrayController, ExperimentalAsh> tray_controller_;

  // Owned by the views hierarchy.
  raw_ptr<FeaturePodButton, ExperimentalAsh> button_ = nullptr;
  raw_ptr<FeatureTile, ExperimentalAsh> tile_ = nullptr;

  absl::optional<int> last_disabled_count_;

  base::WeakPtrFactory<QuietModeFeaturePodController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_QUIET_MODE_FEATURE_POD_CONTROLLER_H_
