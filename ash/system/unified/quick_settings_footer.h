// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_FOOTER_H_
#define ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_FOOTER_H_

#include "ash/ash_export.h"
#include "ash/style/pill_button.h"
#include "ash/system/power/power_status.h"
#include "ash/system/unified/power_button.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/views/view.h"

class PrefRegistrySimple;

namespace ash {

class IconButton;
class PillButton;
class UnifiedSystemTrayController;

// A base class for both `QsBatteryLabelView` and `QsBatteryIconView`. This view
// is a Jellyroll `PillButton` component that has a different icon label spacing
// and right padding than `BatteryInfoViewBase`. It updates by observing
// `PowerStatus`.
class QsBatteryInfoViewBase : public PillButton, public PowerStatus::Observer {
 public:
  explicit QsBatteryInfoViewBase(UnifiedSystemTrayController* controller,
                                 const Type type = Type::kFloatingWithoutIcon,
                                 gfx::VectorIcon* icon = nullptr);
  QsBatteryInfoViewBase(const QsBatteryInfoViewBase&) = delete;
  QsBatteryInfoViewBase& operator=(const QsBatteryInfoViewBase&) = delete;
  ~QsBatteryInfoViewBase() override;

  // Updates the subclass view's ui when `OnPowerStatusChanged`.
  virtual void Update() = 0;

 private:
  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;
};

// A view that shows battery status.
class QsBatteryLabelView : public QsBatteryInfoViewBase {
 public:
  explicit QsBatteryLabelView(UnifiedSystemTrayController* controller);
  QsBatteryLabelView(const QsBatteryLabelView&) = delete;
  QsBatteryLabelView& operator=(const QsBatteryLabelView&) = delete;
  ~QsBatteryLabelView() override;

 private:
  // PillButton:
  void OnThemeChanged() override;

  // QsBatteryInfoViewBase:
  void Update() override;
};

// A view that shows battery icon and charging state when smart charging is
// enabled.
class QsBatteryIconView : public QsBatteryInfoViewBase {
 public:
  explicit QsBatteryIconView(UnifiedSystemTrayController* controller);
  QsBatteryIconView(const QsBatteryIconView&) = delete;
  QsBatteryIconView& operator=(const QsBatteryIconView&) = delete;
  ~QsBatteryIconView() override;

 private:
  // PillButton:
  void OnThemeChanged() override;

  // QsBatteryInfoViewBase:
  void Update() override;

  // Builds the battery icon image.
  void ConfigureIcon();
};

// The footer view shown on the the bottom of the `QuickSettingsView`.
class ASH_EXPORT QuickSettingsFooter : public views::View {
 public:
  METADATA_HEADER(QuickSettingsFooter);

  explicit QuickSettingsFooter(UnifiedSystemTrayController* controller);
  QuickSettingsFooter(const QuickSettingsFooter&) = delete;
  QuickSettingsFooter& operator=(const QuickSettingsFooter&) = delete;
  ~QuickSettingsFooter() override;

  // Registers preferences used by this class in the provided `registry`.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  PowerButton* power_button_for_testing() { return power_button_; }

 private:
  friend class QuickSettingsFooterTest;

  // Disables/Enables the `settings_button_` based on `kSettingsIconEnabled`
  // pref.
  void UpdateSettingsButtonState();

  // Owned.
  raw_ptr<IconButton, ExperimentalAsh> settings_button_ = nullptr;

  // Owned by views hierarchy.
  raw_ptr<PowerButton, ExperimentalAsh> power_button_ = nullptr;
  raw_ptr<PillButton, ExperimentalAsh> sign_out_button_ = nullptr;

  // The registrar used to watch prefs changes.
  PrefChangeRegistrar local_state_pref_change_registrar_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_FOOTER_H_
