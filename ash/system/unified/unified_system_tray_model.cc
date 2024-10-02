// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_model.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/status_area_widget.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"

namespace {

// The minimum width for system tray with size of kMedium.
constexpr int kMinWidthMediumSystemTray = 768;

// The maximum width for system tray with size of kMedium.
constexpr int kMaxWidthMediumSystemTray = 1280;

}  // namespace

namespace ash {

class UnifiedSystemTrayModel::DBusObserver
    : public chromeos::PowerManagerClient::Observer {
 public:
  explicit DBusObserver(UnifiedSystemTrayModel* owner);

  DBusObserver(const DBusObserver&) = delete;
  DBusObserver& operator=(const DBusObserver&) = delete;

  ~DBusObserver() override;

 private:
  void HandleInitialBrightness(absl::optional<double> percent);

  // chromeos::PowerManagerClient::Observer:
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;
  void KeyboardBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;

  const raw_ptr<UnifiedSystemTrayModel, ExperimentalAsh> owner_;

  base::WeakPtrFactory<DBusObserver> weak_ptr_factory_{this};
};

class UnifiedSystemTrayModel::SizeObserver : public display::DisplayObserver,
                                             public ShellObserver {
 public:
  explicit SizeObserver(UnifiedSystemTrayModel* owner);
  ~SizeObserver() override;
  SizeObserver(const SizeObserver&) = delete;
  SizeObserver& operator=(const SizeObserver&) = delete;

 private:
  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  void Update();

  const raw_ptr<UnifiedSystemTrayModel, ExperimentalAsh> owner_;

  display::ScopedDisplayObserver display_observer_{this};

  // Keep track of current system tray size.
  UnifiedSystemTrayModel::SystemTrayButtonSize system_tray_size_;
};

UnifiedSystemTrayModel::DBusObserver::DBusObserver(
    UnifiedSystemTrayModel* owner)
    : owner_(owner) {
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  Shell::Get()->brightness_control_delegate()->GetBrightnessPercent(
      base::BindOnce(&DBusObserver::HandleInitialBrightness,
                     weak_ptr_factory_.GetWeakPtr()));
}

UnifiedSystemTrayModel::DBusObserver::~DBusObserver() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void UnifiedSystemTrayModel::DBusObserver::HandleInitialBrightness(
    absl::optional<double> percent) {
  if (percent.has_value())
    owner_->DisplayBrightnessChanged(percent.value() / 100.,
                                     false /* by_user */);
}

void UnifiedSystemTrayModel::DBusObserver::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  owner_->DisplayBrightnessChanged(
      change.percent() / 100.,
      change.cause() ==
          power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
}

void UnifiedSystemTrayModel::DBusObserver::KeyboardBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  owner_->KeyboardBrightnessChanged(change.percent() / 100., change.cause());
}

UnifiedSystemTrayModel::SizeObserver::SizeObserver(
    UnifiedSystemTrayModel* owner)
    : owner_(owner) {
  Shell::Get()->AddShellObserver(this);
  system_tray_size_ = owner_->GetSystemTrayButtonSize();
}

UnifiedSystemTrayModel::SizeObserver::~SizeObserver() {
  Shell::Get()->RemoveShellObserver(this);
}

void UnifiedSystemTrayModel::SizeObserver::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (owner_->GetDisplay().id() != display.id())
    return;
  Update();
}

void UnifiedSystemTrayModel::SizeObserver::OnShelfAlignmentChanged(
    aura::Window* root_window,
    ShelfAlignment old_alignment) {
  Update();
}

void UnifiedSystemTrayModel::SizeObserver::Update() {
  UnifiedSystemTrayModel::SystemTrayButtonSize new_size =
      owner_->GetSystemTrayButtonSize();
  if (system_tray_size_ == new_size)
    return;

  system_tray_size_ = new_size;
  owner_->SystemTrayButtonSizeChanged(system_tray_size_);
}

UnifiedSystemTrayModel::UnifiedSystemTrayModel(Shelf* shelf)
    : shelf_(shelf),
      dbus_observer_(std::make_unique<DBusObserver>(this)),
      size_observer_(std::make_unique<SizeObserver>(this)) {
  // |shelf_| might be null in unit tests.
  pagination_model_ = std::make_unique<PaginationModel>(
      shelf_ ? shelf_->GetStatusAreaWidget()->GetRootView() : nullptr);
}

UnifiedSystemTrayModel::~UnifiedSystemTrayModel() = default;

void UnifiedSystemTrayModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UnifiedSystemTrayModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool UnifiedSystemTrayModel::IsExpandedOnOpen() const {
  // They System Tray is always expanded with QsRevamp enabled.
  if (features::IsQsRevampEnabled()) {
    return true;
  }

  return expanded_on_open_ != StateOnOpen::COLLAPSED ||
         Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

bool UnifiedSystemTrayModel::IsExplicitlyExpanded() const {
  return expanded_on_open_ == StateOnOpen::EXPANDED;
}

absl::optional<bool> UnifiedSystemTrayModel::GetNotificationExpanded(
    const std::string& notification_id) const {
  auto it = notification_changes_.find(notification_id);
  return it == notification_changes_.end() ? absl::optional<bool>()
                                           : absl::optional<bool>(it->second);
}

void UnifiedSystemTrayModel::SetTargetNotification(
    const std::string& notification_id) {
  DCHECK(!notification_id.empty());
  notification_target_id_ = notification_id;
  notification_target_mode_ = NotificationTargetMode::NOTIFICATION_ID;
}

void UnifiedSystemTrayModel::SetNotificationExpanded(
    const std::string& notification_id,
    bool expanded) {
  notification_changes_[notification_id] = expanded;
}

void UnifiedSystemTrayModel::RemoveNotificationExpanded(
    const std::string& notification_id) {
  notification_changes_.erase(notification_id);
}

void UnifiedSystemTrayModel::ClearNotificationChanges() {
  notification_changes_.clear();
}

UnifiedSystemTrayModel::SystemTrayButtonSize
UnifiedSystemTrayModel::GetSystemTrayButtonSize() const {
  // |shelf_| might be null in unit tests, returns medium size as default.
  if (!shelf_)
    return SystemTrayButtonSize::kMedium;

  // Handles the cases: the shelf is placed horizontally or vertically, or the
  // screen is rotated.
  const int display_size =
      std::max(GetDisplay().size().width(), GetDisplay().size().height());

  if (display_size < kMinWidthMediumSystemTray)
    return SystemTrayButtonSize::kSmall;
  if (display_size <= kMaxWidthMediumSystemTray)
    return SystemTrayButtonSize::kMedium;
  return SystemTrayButtonSize::kLarge;
}

void UnifiedSystemTrayModel::DisplayBrightnessChanged(float brightness,
                                                      bool by_user) {
  display_brightness_ = brightness;
  for (auto& observer : observers_)
    observer.OnDisplayBrightnessChanged(by_user);
}

void UnifiedSystemTrayModel::KeyboardBrightnessChanged(
    float brightness,
    power_manager::BacklightBrightnessChange_Cause cause) {
  keyboard_brightness_ = brightness;
  for (auto& observer : observers_)
    observer.OnKeyboardBrightnessChanged(cause);
}

void UnifiedSystemTrayModel::SystemTrayButtonSizeChanged(
    SystemTrayButtonSize system_tray_size) {
  for (auto& observer : observers_)
    observer.OnSystemTrayButtonSizeChanged(system_tray_size);
}

const display::Display UnifiedSystemTrayModel::GetDisplay() const {
  // |shelf_| might be null in unit tests, returns primary display as default.
  if (!shelf_)
    return display::Screen::GetScreen()->GetPrimaryDisplay();

  return display::Screen::GetScreen()->GetDisplayNearestWindow(
      shelf_->GetStatusAreaWidget()
          ->GetRootView()
          ->GetWidget()
          ->GetNativeWindow());
}

}  // namespace ash
