// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_UTIL_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_UTIL_H_

#import <Foundation/Foundation.h>

@class UIApplication;
@class UNNotificationCategory;
@class UNNotificationSettings;

// This collection of class functions' purpose is to encapsulate the push
// notification functionality that must interact with Apple in some manner,
// either by interacting with iOS or Apple Push Notification Service (APNS).
@interface PushNotificationUtil : NSObject

// The function registers the device with APNS. AppDelegate's
// didRegisterForNotificationsWithDeviceToken function is called if the device
// was successfully registered with APNS. If the device's registration was
// unsuccessful, then AppDelegate's didRegisterForNotificationsWithError
// function is called.
+ (void)registerDeviceWithAPNS;

// The function registers the set of `UNNotificationCategory` objects with iOS'
// UNNotificationCenter.
+ (void)registerActionableNotifications:
    (NSSet<UNNotificationCategory*>*)categories;

// This function displays a permission request system prompt. On display of this
// prompt, the user must decide whether or not to allow iOS to notify them of
// incoming Chromium push notifications. If the user decides to allow push
// notifications, then `completionHandler` is executed with `granted` equaling
// `true`. Also, there is a possibility that `completionHandler` will be
// executed in a background thread. In addition, this function reports
// permission request's outcome to metrics. Since iOS only allows applications
// to prompt users for push notifications permissons once, the `promptShown`
// parameter captures whether the permission prompt was indeed displayed to the
// user. If the prompt is not displayed, then `granted` is equal to the user's
// iOS push notification permission setting for Chrome.
+ (void)requestPushNotificationPermission:
    (void (^)(BOOL granted, BOOL promptShown, NSError* error))completionHandler;

// This functions retrieves the authorization and feature-related settings for
// push notifications.
+ (void)getPermissionSettings:
    (void (^)(UNNotificationSettings* settings))completionHandler;

@end

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_UTIL_H_
