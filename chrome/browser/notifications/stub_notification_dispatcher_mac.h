// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_DISPATCHER_MAC_H_
#define CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_DISPATCHER_MAC_H_

#include <vector>

#include "chrome/browser/notifications/notification_dispatcher_mac.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"

class StubNotificationDispatcherMac : public NotificationDispatcherMac {
 public:
  StubNotificationDispatcherMac();
  StubNotificationDispatcherMac(const StubNotificationDispatcherMac&) = delete;
  StubNotificationDispatcherMac& operator=(
      const StubNotificationDispatcherMac&) = delete;
  ~StubNotificationDispatcherMac() override;

  // NotificationDispatcherMac:
  void DisplayNotification(
      NotificationHandler::Type notification_type,
      Profile* profile,
      const message_center::Notification& notification) override;
  void CloseNotificationWithId(
      const MacNotificationIdentifier& identifier) override;
  void CloseNotificationsWithProfileId(const std::string& profile_id,
                                       bool incognito) override;
  void CloseAllNotifications() override;
  void GetDisplayedNotificationsForProfileId(
      const std::string& profile_id,
      bool incognito,
      GetDisplayedNotificationsCallback callback) override;
  void GetAllDisplayedNotifications(
      GetAllDisplayedNotificationsCallback callback) override;

  const std::vector<mac_notifications::mojom::NotificationPtr>& notifications()
      const {
    return notifications_;
  }

 private:
  std::vector<mac_notifications::mojom::NotificationPtr> notifications_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_DISPATCHER_MAC_H_
