// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_DISPLAY_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_DISPLAY_SERVICE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/common/notifications/notification_operation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/public/cpp/notification.h"

namespace content {
class BrowserContext;
}

class Profile;

// Implementation of the NotificationDisplayService interface that can be used
// for testing purposes. Supports additional methods enabling instrumenting the
// faked underlying notification system.
class StubNotificationDisplayService : public NotificationDisplayServiceImpl {
 public:
  // Factory function to be used with NotificationDisplayServiceFactory's
  // SetTestingFactory method, overriding the default display service.
  static std::unique_ptr<KeyedService> FactoryForTests(
      content::BrowserContext* browser_context);

  typedef base::RepeatingCallback<void(
      NotificationOperation operation,
      NotificationHandler::Type notification_type,
      const GURL& origin,
      const std::string& notification_id,
      const absl::optional<int>& action_index,
      const absl::optional<std::u16string>& reply,
      const absl::optional<bool>& by_user)>
      ProcessNotificationOperationCallback;

  explicit StubNotificationDisplayService(Profile* profile);
  StubNotificationDisplayService(const StubNotificationDisplayService&) =
      delete;
  StubNotificationDisplayService& operator=(
      const StubNotificationDisplayService&) = delete;
  ~StubNotificationDisplayService() override;

  // Sets |closure| to be invoked when any notification has been added.
  void SetNotificationAddedClosure(base::RepeatingClosure closure);

  // Sets |closure| to be invoked when any notification has been closed.
  void SetNotificationClosedClosure(base::RepeatingClosure closure);

  // Returns a vector of the displayed Notification objects.
  std::vector<message_center::Notification> GetDisplayedNotificationsForType(
      NotificationHandler::Type type) const;

  absl::optional<message_center::Notification> GetNotification(
      const std::string& notification_id);

  const NotificationCommon::Metadata* GetMetadataForNotification(
      const message_center::Notification& notification);

  // Simulates the notification identified by |notification_id| being clicked
  // on, optionally with the given |action_index| and |reply|.
  void SimulateClick(NotificationHandler::Type notification_type,
                     const std::string& notification_id,
                     absl::optional<int> action_index,
                     absl::optional<std::u16string> reply);

  // Simulates a click on the settings button of the notification identified by
  // |notification_id|.
  void SimulateSettingsClick(NotificationHandler::Type notification_type,
                             const std::string& notification_id);

  // Simulates the notification identified by |notification_id| being closed due
  // to external events, such as the user dismissing it when |by_user| is set.
  // Will wait for the close event to complete. When |silent| is set, the
  // notification handlers won't be informed of the change to immitate behaviour
  // of operating systems that don't inform apps about removed notifications.
  void RemoveNotification(NotificationHandler::Type notification_type,
                          const std::string& notification_id,
                          bool by_user,
                          bool silent);

  // Removes all notifications shown by this display service. Will wait for the
  // close events to complete.
  void RemoveAllNotifications(NotificationHandler::Type notification_type,
                              bool by_user);

  void SetProcessNotificationOperationDelegate(
      const ProcessNotificationOperationCallback& delegate);

  // NotificationDisplayService implementation:
  void Display(NotificationHandler::Type notification_type,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;
  void Close(NotificationHandler::Type notification_type,
             const std::string& notification_id) override;
  void GetDisplayed(DisplayedNotificationsCallback callback) override;
  void ProcessNotificationOperation(
      NotificationOperation operation,
      NotificationHandler::Type notification_type,
      const GURL& origin,
      const std::string& notification_id,
      const absl::optional<int>& action_index,
      const absl::optional<std::u16string>& reply,
      const absl::optional<bool>& by_user) override;

 private:
  // Data to store for a notification that's being shown through this service.
  struct NotificationData {
    NotificationData(NotificationHandler::Type type,
                     const message_center::Notification& notification,
                     std::unique_ptr<NotificationCommon::Metadata> metadata);
    NotificationData(NotificationData&& other);
    ~NotificationData();

    NotificationData& operator=(NotificationData&& other);

    NotificationHandler::Type type;
    message_center::Notification notification;
    std::unique_ptr<NotificationCommon::Metadata> metadata;
  };

  // Returns an iterator to the notification matching the given properties. If
  // there is no notification that matches, returns the end() iterator.
  std::vector<NotificationData>::iterator FindNotification(
      NotificationHandler::Type notification_type,
      const std::string& notification_id);

  base::RepeatingClosure notification_added_closure_;
  base::RepeatingClosure notification_closed_closure_;
  std::vector<NotificationData> notifications_;
  raw_ptr<Profile> profile_;

  ProcessNotificationOperationCallback process_notification_operation_delegate_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_DISPLAY_SERVICE_H_
