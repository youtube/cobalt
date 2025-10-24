// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/history_sync/history_sync_signin_coordinator.h"

#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/authentication/ui_bundled/history_sync/history_sync_popup_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

@interface HistorySyncSigninCoordinator () <HistorySyncPopupCoordinatorDelegate>
@end

@implementation HistorySyncSigninCoordinator {
  HistorySyncPopupCoordinator* _syncPopupCoordinator;
}

- (void)start {
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(self.profile);
  CHECK(identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  syncer::SyncService* syncService =
      SyncServiceFactory::GetForProfile(self.profile);
  syncer::SyncUserSettings* userSettings = syncService->GetUserSettings();
  BOOL alreadyOptIn = userSettings->GetSelectedTypes().HasAll(
      {syncer::UserSelectableType::kHistory,
       syncer::UserSelectableType::kTabs});
  CHECK(!alreadyOptIn);

  [super start];
  _syncPopupCoordinator = [[HistorySyncPopupCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                   showUserEmail:NO
               signOutIfDeclined:NO
                      isOptional:NO
                    contextStyle:self.contextStyle
                     accessPoint:self.accessPoint];
  _syncPopupCoordinator.delegate = self;
  [_syncPopupCoordinator start];
}

#pragma mark - AnimatedCoordinator

- (void)stopAnimated:(BOOL)animated {
  [_syncPopupCoordinator stopAnimated:animated];
  _syncPopupCoordinator.delegate = nil;
  _syncPopupCoordinator = nil;
  [super stopAnimated:animated];
}

#pragma mark - HistorySyncPopupCoordinatorDelegate

- (void)historySyncPopupCoordinator:(HistorySyncPopupCoordinator*)coordinator
                didFinishWithResult:(HistorySyncResult)result {
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(
          self.profile->GetOriginalProfile());
  id<SystemIdentity> primaryIdentity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  SigninCoordinatorResult signinResult;
  switch (result) {
    case HistorySyncResult::kSuccess:
    case HistorySyncResult::kUserCanceled:
    case HistorySyncResult::kSkipped:
      signinResult = SigninCoordinatorResultSuccess;
      CHECK(primaryIdentity, base::NotFatalUntil::M145);
      break;
    case HistorySyncResult::kPrimaryIdentityRemoved:
      signinResult = SigninCoordinatorResultInterrupted;
      CHECK(!primaryIdentity, base::NotFatalUntil::M145);
      break;
  }
  [self runCompletionWithSigninResult:signinResult
                   completionIdentity:primaryIdentity];
}

@end
