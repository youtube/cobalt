// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wilco_dtc_supportd/wilco_dtc_supportd_notification_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

namespace {

const char kNotifierWilco[] = "ash.wilco";

const char kWilcoDtcSupportdNotificationIdBatteryAuth[] = "BatteryAuth";
const char kWilcoDtcSupportdNotificationIdNonWilcoCharger[] = "NonWilcoCharger";
const char kWilcoDtcSupportdNotificationIdIncompatibleDock[] =
    "IncompatibleDock";
const char kWilcoDtcSupportdNotificationIdDockHardwareError[] = "DockError";
const char kWilcoDtcSupportdNotificationIdDockDisplay[] = "DockDisplay";
const char kWilcoDtcSupportdNotificationIdDockThunderbolt[] = "DockThunderbolt";
const char kWilcoDtcSupportdNotificationIdLowPower[] = "LowPower";

class WilcoDtcSupportdNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  explicit WilcoDtcSupportdNotificationDelegate(
      HelpAppLauncher::HelpTopic topic)
      : topic_(topic) {}
  WilcoDtcSupportdNotificationDelegate(
      const WilcoDtcSupportdNotificationDelegate& other) = delete;
  WilcoDtcSupportdNotificationDelegate& operator=(
      const WilcoDtcSupportdNotificationDelegate& other) = delete;

  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override {
    if (button_index && *button_index == 0) {
      auto help_app(
          base::MakeRefCounted<HelpAppLauncher>(nullptr /* parent_window */));
      help_app->ShowHelpTopic(topic_);
    }
  }

 private:
  ~WilcoDtcSupportdNotificationDelegate() override = default;

  const HelpAppLauncher::HelpTopic topic_;
};

}  // namespace

WilcoDtcSupportdNotificationController::WilcoDtcSupportdNotificationController()
    : WilcoDtcSupportdNotificationController(
          g_browser_process->profile_manager()) {}

WilcoDtcSupportdNotificationController::WilcoDtcSupportdNotificationController(
    ProfileManager* profile_manager)
    : profile_manager_(profile_manager) {
  DCHECK(profile_manager_);
}

WilcoDtcSupportdNotificationController::
    ~WilcoDtcSupportdNotificationController() = default;

std::string
WilcoDtcSupportdNotificationController::ShowBatteryAuthNotification() const {
  DisplayNotification(kWilcoDtcSupportdNotificationIdBatteryAuth,
                      NotificationCatalogName::kUnauthorizedBattery,
                      IDS_WILCO_NOTIFICATION_BATTERY_AUTH_TITLE,
                      IDS_WILCO_NOTIFICATION_BATTERY_AUTH_MESSAGE,
                      message_center::SYSTEM_PRIORITY, kNotificationBatteryIcon,
                      message_center::SystemNotificationWarningLevel::WARNING,
                      HelpAppLauncher::HelpTopic::HELP_WILCO_BATTERY_CHARGER);
  return kWilcoDtcSupportdNotificationIdBatteryAuth;
}

std::string
WilcoDtcSupportdNotificationController::ShowNonWilcoChargerNotification()
    const {
  DisplayNotification(kWilcoDtcSupportdNotificationIdNonWilcoCharger,
                      NotificationCatalogName::kNonWilcoCharger,
                      IDS_WILCO_NOTIFICATION_NON_WILCO_CHARGER_TITLE,
                      IDS_WILCO_NOTIFICATION_NON_WILCO_CHARGER_MESSAGE,
                      message_center::DEFAULT_PRIORITY,
                      kNotificationBatteryIcon,
                      message_center::SystemNotificationWarningLevel::WARNING,
                      HelpAppLauncher::HelpTopic::HELP_WILCO_BATTERY_CHARGER);
  return kWilcoDtcSupportdNotificationIdNonWilcoCharger;
}

std::string
WilcoDtcSupportdNotificationController::ShowIncompatibleDockNotification()
    const {
  DisplayNotification(kWilcoDtcSupportdNotificationIdIncompatibleDock,
                      NotificationCatalogName::kIncompatibleDock,
                      IDS_WILCO_NOTIFICATION_INCOMPATIBLE_DOCK_TITLE,
                      IDS_WILCO_NOTIFICATION_INCOMPATIBLE_DOCK_MESSAGE,
                      message_center::DEFAULT_PRIORITY,
                      vector_icons::kSettingsIcon,
                      message_center::SystemNotificationWarningLevel::NORMAL,
                      HelpAppLauncher::HelpTopic::HELP_WILCO_DOCK);
  return kWilcoDtcSupportdNotificationIdIncompatibleDock;
}

std::string WilcoDtcSupportdNotificationController::ShowDockErrorNotification()
    const {
  DisplayNotification(kWilcoDtcSupportdNotificationIdDockHardwareError,
                      NotificationCatalogName::kDockError,
                      IDS_WILCO_NOTIFICATION_DOCK_ERROR_TITLE,
                      IDS_WILCO_NOTIFICATION_DOCK_ERROR_MESSAGE,
                      message_center::DEFAULT_PRIORITY,
                      vector_icons::kSettingsIcon,
                      message_center::SystemNotificationWarningLevel::NORMAL,
                      HelpAppLauncher::HelpTopic::HELP_WILCO_DOCK);
  return kWilcoDtcSupportdNotificationIdDockHardwareError;
}

std::string
WilcoDtcSupportdNotificationController::ShowDockDisplayNotification() const {
  DisplayNotification(kWilcoDtcSupportdNotificationIdDockDisplay,
                      NotificationCatalogName::KDockDisplayError,
                      IDS_WILCO_NOTIFICATION_DOCK_DISPLAY_TITLE,
                      IDS_WILCO_NOTIFICATION_DOCK_DISPLAY_MESSAGE,
                      message_center::DEFAULT_PRIORITY,
                      vector_icons::kSettingsIcon,
                      message_center::SystemNotificationWarningLevel::NORMAL,
                      HelpAppLauncher::HelpTopic::HELP_WILCO_DOCK);
  return kWilcoDtcSupportdNotificationIdDockDisplay;
}

std::string
WilcoDtcSupportdNotificationController::ShowDockThunderboltNotification()
    const {
  DisplayNotification(kWilcoDtcSupportdNotificationIdDockThunderbolt,
                      NotificationCatalogName::kDockThunderboltError,
                      IDS_WILCO_NOTIFICATION_DOCK_THUNDERBOLT_TITLE,
                      IDS_WILCO_NOTIFICATION_DOCK_THUNDERBOLT_MESSAGE,
                      message_center::DEFAULT_PRIORITY,
                      vector_icons::kSettingsIcon,
                      message_center::SystemNotificationWarningLevel::NORMAL,
                      HelpAppLauncher::HelpTopic::HELP_WILCO_DOCK);
  return kWilcoDtcSupportdNotificationIdDockThunderbolt;
}

std::string
WilcoDtcSupportdNotificationController::ShowLowPowerChargerNotification()
    const {
  DisplayNotification(kWilcoDtcSupportdNotificationIdLowPower,
                      NotificationCatalogName::kWilcoLowPowerCharger,
                      IDS_WILCO_LOW_POWER_CHARGER_TITLE,
                      IDS_WILCO_LOW_POWER_CHARGER_MESSAGE,
                      message_center::SYSTEM_PRIORITY, kNotificationBatteryIcon,
                      message_center::SystemNotificationWarningLevel::WARNING,
                      HelpAppLauncher::HelpTopic::HELP_WILCO_BATTERY_CHARGER);
  return kWilcoDtcSupportdNotificationIdLowPower;
}

void WilcoDtcSupportdNotificationController::DisplayNotification(
    const std::string& notification_id,
    const NotificationCatalogName& catalog_name,
    const int title_id,
    const int message_id,
    const message_center::NotificationPriority priority,
    const gfx::VectorIcon& small_image,
    const message_center::SystemNotificationWarningLevel color_type,
    const HelpAppLauncher::HelpTopic topic) const {
  message_center::RichNotificationData rich_data;
  rich_data.pinned = (priority == message_center::SYSTEM_PRIORITY);
  message_center::Notification notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(title_id),
      l10n_util::GetStringUTF16(message_id),
      std::u16string() /* display_source */, GURL() /* origin_url */,
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierWilco, catalog_name),
      rich_data,
      base::MakeRefCounted<WilcoDtcSupportdNotificationDelegate>(topic),
      small_image, color_type);
  notification.set_buttons({message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_WILCO_NOTIFICATION_LEARN_MORE))});
  if (priority == message_center::SYSTEM_PRIORITY) {
    notification.SetSystemPriority();
  }
  NotificationDisplayService::GetForProfile(
      profile_manager_->GetLastUsedProfile())
      ->Display(NotificationHandler::Type::TRANSIENT, notification,
                nullptr /* metadata */);
}

}  // namespace ash
