// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/notifications_internals/notifications_internals_ui_page_handler.h"

#include "base/functional/bind.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/tips/core/tips_types.h"
#include "chrome/browser/tips/core/tips_utils.h"
#include "chrome/browser/ui/webui/notifications_internals/notifications_internals.mojom.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

NotificationsInternalsUIPageHandler::NotificationsInternalsUIPageHandler(
    mojo::PendingReceiver<notifications_internals::mojom::PageHandler> receiver,
    notifications::NotificationScheduleService* service,
    PrefService* pref_service)
    : receiver_(this, std::move(receiver)),
      service_(service),
      pref_service_(pref_service) {}

NotificationsInternalsUIPageHandler::~NotificationsInternalsUIPageHandler() =
    default;

void NotificationsInternalsUIPageHandler::ScheduleNotification(
    const std::string& feature_type) {
  tips::TipsNotificationsFeatureType type;
  if (feature_type == "esb") {
    type = tips::TipsNotificationsFeatureType::kEnhancedSafeBrowsing;
  } else if (feature_type == "quick_delete") {
    type = tips::TipsNotificationsFeatureType::kQuickDelete;
  } else if (feature_type == "google_lens") {
    type = tips::TipsNotificationsFeatureType::kGoogleLens;
  } else if (feature_type == "bottom_omnibox") {
    type = tips::TipsNotificationsFeatureType::kBottomOmnibox;
  } else if (feature_type == "password_autofill") {
    type = tips::TipsNotificationsFeatureType::kPasswordAutofill;
  } else if (feature_type == "signin") {
    type = tips::TipsNotificationsFeatureType::kSignin;
  } else if (feature_type == "create_tab_groups") {
    type = tips::TipsNotificationsFeatureType::kCreateTabGroups;
  } else if (feature_type == "customize_mvt") {
    type = tips::TipsNotificationsFeatureType::kCustomizeMVT;
  } else if (feature_type == "recent_tabs") {
    type = tips::TipsNotificationsFeatureType::kRecentTabs;
  } else {
    NOTREACHED();
  }

  notifications::ScheduleParams schedule_params;
  schedule_params.priority =
      notifications::ScheduleParams::Priority::kNoThrottle;
  schedule_params.deliver_time_start = base::Time::Now();
  schedule_params.deliver_time_end = base::Time::Now() + base::Minutes(1);
  notifications::NotificationData data = tips::GetTipsNotificationData(type);
  auto params = std::make_unique<notifications::NotificationParams>(
      notifications::SchedulerClientType::kTips, std::move(data),
      std::move(schedule_params));
  service_->Schedule(std::move(params));
}
