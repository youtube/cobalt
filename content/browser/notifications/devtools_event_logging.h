// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_DEVTOOLS_EVENT_LOGGING_H_
#define CONTENT_BROWSER_NOTIFICATIONS_DEVTOOLS_EVENT_LOGGING_H_

#include <string>

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace content {

class BrowserContext;
struct NotificationDatabaseData;

namespace notifications {

bool ShouldLogNotificationEventToDevTools(BrowserContext* browser_context,
                                          const GURL& origin);

void LogNotificationDisplayedEventToDevTools(
    BrowserContext* browser_context,
    const NotificationDatabaseData& data);

void LogNotificationClosedEventToDevTools(BrowserContext* browser_context,
                                          const NotificationDatabaseData& data);

void LogNotificationScheduledEventToDevTools(
    BrowserContext* browser_context,
    const NotificationDatabaseData& data,
    base::Time show_trigger_timestamp);

void LogNotificationClickedEventToDevTools(
    BrowserContext* browser_context,
    const NotificationDatabaseData& data,
    const absl::optional<int>& action_index,
    const absl::optional<std::u16string>& reply);

}  // namespace notifications
}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_DEVTOOLS_EVENT_LOGGING_H_
