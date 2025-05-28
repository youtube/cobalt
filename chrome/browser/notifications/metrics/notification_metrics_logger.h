// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_METRICS_NOTIFICATION_METRICS_LOGGER_H_
#define CHROME_BROWSER_NOTIFICATIONS_METRICS_NOTIFICATION_METRICS_LOGGER_H_

#include "components/keyed_service/core/keyed_service.h"
#include "content/public/common/persistent_notification_status.h"

// Logs when various notification-related events have occurred.
// Methods are virtual so they can be mocked in tests.
class NotificationMetricsLogger : public KeyedService {
 public:
  NotificationMetricsLogger();
  ~NotificationMetricsLogger() override;

  // Logs that a persistent notification was closed by the user.
  virtual void LogPersistentNotificationClosedByUser();

  // Logs that a persistent notification was closed but not by the user.
  virtual void LogPersistentNotificationClosedProgrammatically();

  // Logs that a persistent notification action button was clicked.
  virtual void LogPersistentNotificationActionButtonClick();

  // Logs that a persistent notification was clicked (with permission).
  virtual void LogPersistentNotificationClick();

  // Logs that a persistent notification was clicked without permission.
  virtual void LogPersistentNotificationClickWithoutPermission();

  // Logs that a persistent notification has been displayed.
  virtual void LogPersistentNotificationShown();
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_METRICS_NOTIFICATION_METRICS_LOGGER_H_"
