// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/push_notification_service.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/push_notification/push_notification_account_context_manager.h"
#import "ios/chrome/browser/push_notification/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/push_notification_client_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PushNotificationService::PushNotificationService()
    : client_manager_(std::make_unique<PushNotificationClientManager>()) {
  ios::ChromeBrowserStateManager* manager =
      GetApplicationContext()->GetChromeBrowserStateManager();
  context_manager_ = [[PushNotificationAccountContextManager alloc]
      initWithChromeBrowserStateManager:manager];
}

PushNotificationService::~PushNotificationService() = default;

PushNotificationClientManager*
PushNotificationService::GetPushNotificationClientManager() {
  return client_manager_.get();
}

PushNotificationAccountContextManager*
PushNotificationService::GetAccountContextManager() {
  return context_manager_;
}

void PushNotificationService::SetPreference(NSString* account_id,
                                            PushNotificationClientId client_id,
                                            bool enabled) {
  DCHECK(context_manager_);
  if (enabled) {
    [context_manager_
        enablePushNotification:client_id
                    forAccount:base::SysNSStringToUTF8(account_id)];
  } else {
    [context_manager_
        disablePushNotification:client_id
                     forAccount:base::SysNSStringToUTF8(account_id)];
  }

  SetPreferences(
      account_id,
      [context_manager_
          preferenceMapForAccount:base::SysNSStringToUTF8(account_id)],
      ^(NSError* error){
      });
}

void PushNotificationService::RegisterAccount(
    NSString* account_id,
    CompletionHandler completion_handler) {
  if ([context_manager_ addAccount:base::SysNSStringToUTF8(account_id)]) {
    SetAccountsToDevice([context_manager_ accountIDs], completion_handler);
  }
}

void PushNotificationService::UnregisterAccount(
    NSString* account_id,
    CompletionHandler completion_handler) {
  if ([context_manager_ removeAccount:base::SysNSStringToUTF8(account_id)]) {
    SetAccountsToDevice([context_manager_ accountIDs], completion_handler);
  }
}

void PushNotificationService::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  base::Value::Dict feature_push_notification_permission = base::Value::Dict();
  registry->RegisterDictionaryPref(
      prefs::kFeaturePushNotificationPermissions,
      std::move(feature_push_notification_permission));
}

void PushNotificationService::SetPreferences(
    NSString* account_id,
    PreferenceMap preference_map,
    CompletionHandler completion_handler) {}
