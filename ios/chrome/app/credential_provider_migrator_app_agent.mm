// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/credential_provider_migrator_app_agent.h"

#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_features_util.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/credential_provider/credential_provider_migrator.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_provider.h"
#import "ios/chrome/browser/main/browser_provider_interface.h"
#import "ios/chrome/browser/passwords/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CredentialProviderAppAgent ()

// `migrator` is in charge of migrating the password when Chrome comes to
// foreground.
@property(nonatomic, strong) CredentialProviderMigrator* migrator;

@end

@implementation CredentialProviderAppAgent

- (void)appDidEnterForeground {
  if (self.migrator) {
    return;
  }
  NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
  SceneState* anyScene = self.appState.foregroundScenes.firstObject;
  DCHECK(anyScene);
  ChromeBrowserState* browserState =
      anyScene.browserProviderInterface.mainBrowserProvider.browser
          ->GetBrowserState();
  DCHECK(browserState);
  password_manager::PasswordForm::Store defaultStore =
      password_manager::features_util::GetDefaultPasswordStore(
          browserState->GetPrefs(),
          SyncServiceFactory::GetForBrowserState(browserState));
  scoped_refptr<password_manager::PasswordStoreInterface> storeToSave =
      defaultStore == password_manager::PasswordForm::Store::kAccountStore
          ? IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
                browserState, ServiceAccessType::IMPLICIT_ACCESS)
          : IOSChromePasswordStoreFactory::GetForBrowserState(
                browserState, ServiceAccessType::IMPLICIT_ACCESS);
  NSUserDefaults* userDefaults = app_group::GetGroupUserDefaults();
  self.migrator =
      [[CredentialProviderMigrator alloc] initWithUserDefaults:userDefaults
                                                           key:key
                                                 passwordStore:storeToSave];
  __weak __typeof__(self) weakSelf = self;
  [self.migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
    DCHECK(success);
    weakSelf.migrator = nil;
  }];
}

@end
