// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/mac_notifications/public/cpp/mac_notification_metrics.h"

#import <Foundation/Foundation.h>

#include "base/mac/bundle_locations.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace mac_notifications {

bool IsAppBundleAlertStyle() {
  NSDictionary* infoDictionary = [base::mac::MainBundle() infoDictionary];
  NSString* alertStyle = infoDictionary[@"NSUserNotificationAlertStyle"];
  return [alertStyle isEqualToString:@"alert"];
}

std::string MacNotificationStyleSuffix(bool is_alert) {
  return is_alert ? "Alert" : "Banner";
}

void LogMacNotificationActionReceived(bool is_alert, bool is_valid) {
  base::UmaHistogramBoolean(
      base::StrCat({"Notifications.macOS.ActionReceived.",
                    MacNotificationStyleSuffix(is_alert)}),
      is_valid);
}

}  // namespace mac_notifications
