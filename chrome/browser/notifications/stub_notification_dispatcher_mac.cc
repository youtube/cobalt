// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/stub_notification_dispatcher_mac.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_utils.h"

StubNotificationDispatcherMac::StubNotificationDispatcherMac() = default;

StubNotificationDispatcherMac::~StubNotificationDispatcherMac() = default;

void StubNotificationDispatcherMac::DisplayNotification(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification) {
  notifications_.push_back(
      CreateMacNotification(notification_type, profile, notification));
}

void StubNotificationDispatcherMac::CloseNotificationWithId(
    const MacNotificationIdentifier& identifier) {
  notifications_.erase(
      std::remove_if(notifications_.begin(), notifications_.end(),
                     [&identifier](const auto& notification) {
                       const auto& notification_id = notification->meta->id->id;
                       const auto& profile = notification->meta->id->profile;
                       return notification_id == identifier.notification_id &&
                              profile->id == identifier.profile_id &&
                              profile->incognito == identifier.incognito;
                     }),
      notifications_.end());
}

void StubNotificationDispatcherMac::CloseNotificationsWithProfileId(
    const std::string& profile_id,
    bool incognito) {
  notifications_.erase(
      std::remove_if(notifications_.begin(), notifications_.end(),
                     [&profile_id, incognito](const auto& notification) {
                       const auto& profile = notification->meta->id->profile;
                       return profile->id == profile_id &&
                              profile->incognito == incognito;
                     }),
      notifications_.end());
}

void StubNotificationDispatcherMac::CloseAllNotifications() {
  notifications_.clear();
}

void StubNotificationDispatcherMac::GetDisplayedNotificationsForProfileId(
    const std::string& profile_id,
    bool incognito,
    GetDisplayedNotificationsCallback callback) {
  std::set<std::string> notifications;
  for (const auto& notification : notifications_) {
    const auto& notification_id = notification->meta->id->id;
    const auto& profile = notification->meta->id->profile;
    if (profile->id == profile_id && profile->incognito == incognito)
      notifications.insert(notification_id);
  }
  std::move(callback).Run(std::move(notifications),
                          /*supports_synchronization=*/true);
}

void StubNotificationDispatcherMac::GetAllDisplayedNotifications(
    GetAllDisplayedNotificationsCallback callback) {
  std::vector<MacNotificationIdentifier> notification_ids;

  for (const auto& notification : notifications_) {
    const auto& notification_id = notification->meta->id->id;
    const auto& profile = notification->meta->id->profile;
    notification_ids.push_back(
        {notification_id, profile->id, profile->incognito});
  }

  // Create set from std::vector to avoid N^2 insertion runtime.
  base::flat_set<MacNotificationIdentifier> notification_set(
      std::move(notification_ids));
  std::move(callback).Run(std::move(notification_set));
}
