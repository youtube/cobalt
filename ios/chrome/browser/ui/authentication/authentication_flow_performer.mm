// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/authentication_flow_performer.h"

#import <memory>

#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/ios/block_types.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "google_apis/gaia/gaia_urls.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/policy/cloud/user_policy_signin_service.h"
#import "ios/chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#import "ios/chrome/browser/policy/cloud/user_policy_switch.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/constants.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/signin/system_identity_manager.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/settings/import_data_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_ui::CompletionCallback;

namespace {

const int64_t kAuthenticationFlowTimeoutSeconds = 10;

}  // namespace

@interface AuthenticationFlowPerformer () <ImportDataControllerDelegate,
                                           SettingsNavigationControllerDelegate>
@end

@implementation AuthenticationFlowPerformer {
  __weak id<AuthenticationFlowPerformerDelegate> _delegate;
  AlertCoordinator* _alertCoordinator;
  SettingsNavigationController* _navigationController;
  std::unique_ptr<base::OneShotTimer> _watchdogTimer;
}

- (id<AuthenticationFlowPerformerDelegate>)delegate {
  return _delegate;
}

- (instancetype)initWithDelegate:
    (id<AuthenticationFlowPerformerDelegate>)delegate {
  self = [super init];
  if (self)
    _delegate = delegate;
  return self;
}

- (void)cancelAndDismissAnimated:(BOOL)animated {
  [_alertCoordinator executeCancelHandler];
  [_alertCoordinator stop];
  if (_navigationController) {
    [_navigationController cleanUpSettings];
    _navigationController = nil;
    [_delegate dismissPresentingViewControllerAnimated:animated completion:nil];
  }
  [self stopWatchdogTimer];
}

- (void)commitSyncForBrowserState:(ChromeBrowserState*)browserState {
  SyncSetupServiceFactory::GetForBrowserState(browserState)
      ->CommitSyncChanges();
}

- (void)fetchManagedStatus:(ChromeBrowserState*)browserState
               forIdentity:(id<SystemIdentity>)identity {
  SystemIdentityManager* systemIdentityManager =
      GetApplicationContext()->GetSystemIdentityManager();
  if (NSString* hostedDomain =
          systemIdentityManager->GetCachedHostedDomainForIdentity(identity)) {
    [_delegate didFetchManagedStatus:hostedDomain];
    return;
  }

  [self startWatchdogTimerForManagedStatus];
  __weak AuthenticationFlowPerformer* weakSelf = self;
  systemIdentityManager->GetHostedDomain(
      identity, base::BindOnce(^(NSString* hostedDomain, NSError* error) {
        [weakSelf handleGetHostedDomain:hostedDomain error:error];
      }));
}

- (void)signInIdentity:(id<SystemIdentity>)identity
      withHostedDomain:(NSString*)hostedDomain
        toBrowserState:(ChromeBrowserState*)browserState {
  AuthenticationServiceFactory::GetForBrowserState(browserState)
      ->SignIn(identity);
}

- (void)signOutBrowserState:(ChromeBrowserState*)browserState {
  __weak __typeof(_delegate) weakDelegate = _delegate;
  AuthenticationServiceFactory::GetForBrowserState(browserState)
      ->SignOut(signin_metrics::ProfileSignout::kUserClickedSignoutSettings,
                /*force_clear_browsing_data=*/false, ^{
                  [weakDelegate didSignOut];
                });
}

- (void)signOutImmediatelyFromBrowserState:(ChromeBrowserState*)browserState {
  AuthenticationServiceFactory::GetForBrowserState(browserState)
      ->SignOut(signin_metrics::ProfileSignout::kAbortSignin,
                /*force_clear_browsing_data=*/false, nil);
}

- (void)promptMergeCaseForIdentity:(id<SystemIdentity>)identity
                           browser:(Browser*)browser
                    viewController:(UIViewController*)viewController {
  DCHECK(browser);
  ChromeBrowserState* browserState = browser->GetBrowserState();
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(browserState);
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  NSString* lastSyncingEmail =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSync)
          .userEmail;
  if (!lastSyncingEmail) {
    // User is not opted in to sync, the current data comes may belong to the
    // previously syncing account (if any).
    lastSyncingEmail =
        base::SysUTF8ToNSString(browserState->GetPrefs()->GetString(
            prefs::kGoogleServicesLastUsername));
  }

  if (authenticationService->HasPrimaryIdentityManaged(
          signin::ConsentLevel::kSync)) {
    // If the current user is a managed account and sync is enabled, the sign-in
    // needs to wipe the current data. We need to ask confirm from the user.
    AccountInfo primaryAccountInfo = identityManager->FindExtendedAccountInfo(
        identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync));
    DCHECK(!primaryAccountInfo.IsEmpty());
    NSString* hostedDomain =
        base::SysUTF8ToNSString(primaryAccountInfo.hosted_domain);
    [self promptSwitchFromManagedEmail:lastSyncingEmail
                      withHostedDomain:hostedDomain
                               toEmail:identity.userEmail
                        viewController:viewController
                               browser:browser];
    return;
  }
  _navigationController = [SettingsNavigationController
      importDataControllerForBrowser:browser
                            delegate:self
                  importDataDelegate:self
                           fromEmail:lastSyncingEmail
                             toEmail:identity.userEmail];
  [_delegate presentViewController:_navigationController
                          animated:YES
                        completion:nil];
}

- (void)clearDataFromBrowser:(Browser*)browser
              commandHandler:(id<BrowsingDataCommands>)handler {
  DCHECK(browser);
  ChromeBrowserState* browserState = browser->GetBrowserState();
  // The user needs to be signed out when clearing the data to avoid deleting
  // data on server side too.
  CHECK(!AuthenticationServiceFactory::GetForBrowserState(browserState)
             ->HasPrimaryIdentity(signin::ConsentLevel::kSignin));

  // Workaround for crbug.com/1003578
  //
  // During the Chrome sign-in flow, if the user chooses to clear the cookies
  // when switching their primary account, then the following actions take
  // place (in the following order):
  //   1. All cookies are cleared.
  //   2. The user is signed in to Chrome (aka the primary account set)
  //   2.a. All requests to Gaia page will include Mirror header.
  //   2.b. Account reconcilor will rebuild the Gaia cookies having the Chrome
  //      primary account as the Gaia default web account.
  //
  // The Gaia sign-in webpage monitors changes to its cookies and reloads the
  // page whenever they change. Reloading the webpage while the cookies are
  // cleared and just before they are rebuilt seems to confuse WKWebView that
  // ends up with a nil URL, which in turns translates in about:blank URL shown
  // in the Omnibox.
  //
  // This CL works around this issue by waiting for 1 second between steps 1
  // and 2 above to allow the WKWebView to initiate the reload after the
  // cookies are cleared.
  WebStateList* webStateList = browser->GetWebStateList();
  web::WebState* activeWebState = webStateList->GetActiveWebState();
  bool activeWebStateHasGaiaOrigin =
      activeWebState &&
      (activeWebState->GetVisibleURL().DeprecatedGetOriginAsURL() ==
       GaiaUrls::GetInstance()->gaia_url());
  int64_t dispatchDelaySecs = activeWebStateHasGaiaOrigin ? 1 : 0;
  __weak __typeof(_delegate) weakDelegate = _delegate;
  [handler removeBrowsingDataForBrowserState:browserState
                                  timePeriod:browsing_data::TimePeriod::ALL_TIME
                                  removeMask:BrowsingDataRemoveMask::REMOVE_ALL
                             completionBlock:^{
                               dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                                            dispatchDelaySecs *
                                                                NSEC_PER_SEC),
                                              dispatch_get_main_queue(), ^{
                                                [weakDelegate didClearData];
                                              });
                             }];
}

- (BOOL)shouldHandleMergeCaseForIdentity:(id<SystemIdentity>)identity
                       browserStatePrefs:(PrefService*)prefs {
  const std::string lastSignedInGaiaId =
      prefs->GetString(prefs::kGoogleServicesLastGaiaId);
  if (!lastSignedInGaiaId.empty()) {
    // Merge case exists if the id of the previously signed in account is
    // different from the one of the account being signed in.
    return lastSignedInGaiaId != base::SysNSStringToUTF8(identity.gaiaID);
  }

  // kGoogleServicesLastGaiaId pref might not have been populated yet,
  // check the old kGoogleServicesLastUsername pref.
  const std::string currentSignedInEmail =
      base::SysNSStringToUTF8(identity.userEmail);
  const std::string lastSignedInEmail =
      prefs->GetString(prefs::kGoogleServicesLastUsername);
  return !lastSignedInEmail.empty() &&
         !gaia::AreEmailsSame(currentSignedInEmail, lastSignedInEmail);
}

- (void)showManagedConfirmationForHostedDomain:(NSString*)hostedDomain
                                viewController:(UIViewController*)viewController
                                       browser:(Browser*)browser {
  DCHECK(!_alertCoordinator);
  BOOL userPolicyEnabled = policy::IsUserPolicyEnabled();
  int titleID = userPolicyEnabled ? IDS_IOS_MANAGED_SYNC_TITLE
                                  : IDS_IOS_MANAGED_SIGNIN_TITLE;
  NSString* title = l10n_util::GetNSString(titleID);
  int subtitleID = userPolicyEnabled
                       ? IDS_IOS_MANAGED_SYNC_WITH_USER_POLICY_SUBTITLE
                       : IDS_IOS_MANAGED_SIGNIN_SUBTITLE;
  NSString* subtitle = l10n_util::GetNSStringF(
      subtitleID, base::SysNSStringToUTF16(hostedDomain));
  NSString* acceptLabel =
      l10n_util::GetNSString(IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON);
  NSString* cancelLabel = l10n_util::GetNSString(IDS_CANCEL);

  _alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:viewController
                                                   browser:browser
                                                     title:title
                                                   message:subtitle];

  __weak AuthenticationFlowPerformer* weakSelf = self;
  __weak AlertCoordinator* weakAlert = _alertCoordinator;

  ProceduralBlock acceptBlock = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;

    // TODO(crbug.com/1326767): Nullify the browser object in the
    // AlertCoordinator when the coordinator is stopped to avoid using the
    // browser object at that moment, in which case the browser object may have
    // been deleted before the callback block is called. This is to avoid
    // potential bad memory accesses.
    Browser* alertedBrowser = weakAlert.browser;
    if (alertedBrowser) {
      PrefService* prefService = alertedBrowser->GetBrowserState()->GetPrefs();
      // TODO(crbug.com/1325115): Remove this line once we determined that the
      // notification isn't needed anymore.
      [strongSelf updateUserPolicyNotificationStatusIfNeeded:prefService];
    }

    [strongSelf alertControllerDidDisappear:weakAlert];
    [[strongSelf delegate] didAcceptManagedConfirmation];
  };
  ProceduralBlock cancelBlock = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf alertControllerDidDisappear:weakAlert];
    [[strongSelf delegate] didCancelManagedConfirmation];
  };

  [_alertCoordinator addItemWithTitle:cancelLabel
                               action:cancelBlock
                                style:UIAlertActionStyleCancel];
  [_alertCoordinator addItemWithTitle:acceptLabel
                               action:acceptBlock
                                style:UIAlertActionStyleDefault];
  [_alertCoordinator setCancelAction:cancelBlock];
  [_alertCoordinator start];
}

- (void)showAuthenticationError:(NSError*)error
                 withCompletion:(ProceduralBlock)callback
                 viewController:(UIViewController*)viewController
                        browser:(Browser*)browser {
  DCHECK(!_alertCoordinator);

  _alertCoordinator = ErrorCoordinatorNoItem(error, viewController, browser);

  __weak AuthenticationFlowPerformer* weakSelf = self;
  __weak AlertCoordinator* weakAlert = _alertCoordinator;
  ProceduralBlock dismissAction = ^{
    [weakSelf alertControllerDidDisappear:weakAlert];
    if (callback)
      callback();
  };

  NSString* okButtonLabel = l10n_util::GetNSString(IDS_OK);
  [_alertCoordinator addItemWithTitle:okButtonLabel
                               action:dismissAction
                                style:UIAlertActionStyleDefault];

  [_alertCoordinator setCancelAction:dismissAction];

  [_alertCoordinator start];
}

- (void)registerUserPolicy:(ChromeBrowserState*)browserState
               forIdentity:(id<SystemIdentity>)identity {
  // Should only fetch user policies when the feature is enabled.
  DCHECK(policy::IsUserPolicyEnabled());

  std::string userEmail = base::SysNSStringToUTF8(identity.userEmail);
  CoreAccountId accountID =
      IdentityManagerFactory::GetForBrowserState(browserState)
          ->PickAccountIdForAccount(base::SysNSStringToUTF8(identity.gaiaID),
                                    userEmail);

  policy::UserPolicySigninService* userPolicyService =
      policy::UserPolicySigninServiceFactory::GetForBrowserState(browserState);

  __weak __typeof(self) weakSelf = self;

  [self startWatchdogTimerForUserPolicyRegistration];
  userPolicyService->RegisterForPolicyWithAccountId(
      userEmail, accountID,
      base::BindOnce(^(const std::string& dmToken,
                       const std::string& clientID) {
        if (![self stopWatchdogTimer]) {
          // Watchdog timer has already fired, don't notify the delegate.
          return;
        }
        [weakSelf.delegate
            didRegisterForUserPolicyWithDMToken:base::SysUTF8ToNSString(dmToken)
                                       clientID:base::SysUTF8ToNSString(
                                                    clientID)];
      }));
}

- (void)fetchUserPolicy:(ChromeBrowserState*)browserState
            withDmToken:(NSString*)dmToken
               clientID:(NSString*)clientID
               identity:(id<SystemIdentity>)identity {
  // Should only fetch user policies when the feature is enabled.
  DCHECK(policy::IsUserPolicyEnabled());

  // Need a `dmToken` and a `clientID` to fetch user policies.
  DCHECK([dmToken length] > 0);
  DCHECK([clientID length] > 0);

  policy::UserPolicySigninService* policy_service =
      policy::UserPolicySigninServiceFactory::GetForBrowserState(browserState);
  const std::string userEmail = base::SysNSStringToUTF8(identity.userEmail);

  AccountId accountID =
      AccountId::FromUserEmailGaiaId(gaia::CanonicalizeEmail(userEmail),
                                     base::SysNSStringToUTF8(identity.gaiaID));

  __weak __typeof(self) weakSelf = self;

  [self startWatchdogTimerForUserPolicyFetch];
  policy_service->FetchPolicyForSignedInUser(
      accountID, base::SysNSStringToUTF8(dmToken),
      base::SysNSStringToUTF8(clientID),
      browserState->GetSharedURLLoaderFactory(),
      base::BindOnce(^(bool success) {
        if (![self stopWatchdogTimer]) {
          // Watchdog timer has already fired, don't notify the delegate.
          return;
        }
        [weakSelf.delegate didFetchUserPolicyWithSuccess:success];
      }));
}

#pragma mark - ImportDataControllerDelegate

- (void)didChooseClearDataPolicy:(ImportDataTableViewController*)controller
                 shouldClearData:(ShouldClearData)shouldClearData {
  DCHECK_NE(SHOULD_CLEAR_DATA_USER_CHOICE, shouldClearData);
  if (shouldClearData == SHOULD_CLEAR_DATA_CLEAR_DATA) {
    base::RecordAction(
        base::UserMetricsAction("Signin_ImportDataPrompt_DontImport"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Signin_ImportDataPrompt_ImportData"));
  }

  __weak AuthenticationFlowPerformer* weakSelf = self;
  ProceduralBlock block = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    strongSelf->_navigationController = nil;
    [[strongSelf delegate] didChooseClearDataPolicy:shouldClearData];
  };
  [_navigationController cleanUpSettings];
  [_delegate dismissPresentingViewControllerAnimated:YES completion:block];
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  base::RecordAction(base::UserMetricsAction("Signin_ImportDataPrompt_Cancel"));

  __weak AuthenticationFlowPerformer* weakSelf = self;
  ProceduralBlock block = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    strongSelf->_navigationController = nil;
    [[strongSelf delegate] didChooseCancel];
  };
  [_navigationController cleanUpSettings];
  [_delegate dismissPresentingViewControllerAnimated:YES completion:block];
}

- (void)settingsWasDismissed {
  base::RecordAction(base::UserMetricsAction("Signin_ImportDataPrompt_Cancel"));
  [self.delegate didChooseCancel];
  [_navigationController cleanUpSettings];
  _navigationController = nil;
}

- (id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>)
    handlerForSettings {
  NOTREACHED();
  return nil;
}

- (id<ApplicationCommands>)handlerForApplicationCommands {
  NOTREACHED();
  return nil;
}

- (id<SnackbarCommands>)handlerForSnackbarCommands {
  NOTREACHED();
  return nil;
}

#pragma mark - Private

- (void)updateUserPolicyNotificationStatusIfNeeded:(PrefService*)prefService {
  if (!policy::IsUserPolicyEnabled()) {
    // Don't set the notification pref if the User Policy feature isn't
    // enabled.
    return;
  }

  prefService->SetBoolean(policy::policy_prefs::kUserPolicyNotificationWasShown,
                          true);
}

- (void)handleGetHostedDomain:(NSString*)hostedDomain error:(NSError*)error {
  if (![self stopWatchdogTimer]) {
    // Watchdog timer has already fired, don't notify the delegate.
    return;
  }
  if (error) {
    [_delegate didFailFetchManagedStatus:error];
    return;
  }
  [_delegate didFetchManagedStatus:hostedDomain];
}

// Starts a Watchdog Timer that calls `timeoutBlock` on time out.
- (void)startWatchdogTimerWithTimeoutBlock:(ProceduralBlock)timeoutBlock {
  DCHECK(!_watchdogTimer);
  _watchdogTimer.reset(new base::OneShotTimer());
  _watchdogTimer->Start(FROM_HERE,
                        base::Seconds(kAuthenticationFlowTimeoutSeconds),
                        base::BindOnce(timeoutBlock));
}

// Starts the watchdog timer with a timeout of
// `kAuthenticationFlowTimeoutSeconds` for the fetching managed status
// operation. It will notify `_delegate` of the failure unless
// `stopWatchdogTimer` is called before it times out.
- (void)startWatchdogTimerForManagedStatus {
  __weak AuthenticationFlowPerformer* weakSelf = self;
  ProceduralBlock timeoutBlock = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf stopWatchdogTimer];
    NSError* error = [NSError errorWithDomain:kAuthenticationErrorDomain
                                         code:TIMED_OUT_FETCH_POLICY
                                     userInfo:nil];
    [strongSelf->_delegate didFailFetchManagedStatus:error];
  };
  [self startWatchdogTimerWithTimeoutBlock:timeoutBlock];
}

// Starts a Watchdog Timer that ends the user policy registration on time out.
- (void)startWatchdogTimerForUserPolicyRegistration {
  __weak AuthenticationFlowPerformer* weakSelf = self;
  ProceduralBlock timeoutBlock = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf stopWatchdogTimer];
    [strongSelf.delegate didRegisterForUserPolicyWithDMToken:@"" clientID:@""];
  };
  [self startWatchdogTimerWithTimeoutBlock:timeoutBlock];
}

// Starts a Watchdog Timer that ends the user policy fetch on time out.
- (void)startWatchdogTimerForUserPolicyFetch {
  __weak AuthenticationFlowPerformer* weakSelf = self;
  ProceduralBlock timeoutBlock = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf stopWatchdogTimer];
    [strongSelf->_delegate didFetchUserPolicyWithSuccess:NO];
  };
  [self startWatchdogTimerWithTimeoutBlock:timeoutBlock];
}

// Stops the watchdog timer, and doesn't call the `timeoutDelegateSelector`.
// Returns whether the watchdog was actually running.
- (BOOL)stopWatchdogTimer {
  if (_watchdogTimer) {
    _watchdogTimer->Stop();
    _watchdogTimer.reset();
    return YES;
  }
  return NO;
}

- (void)promptSwitchFromManagedEmail:(NSString*)managedEmail
                    withHostedDomain:(NSString*)hostedDomain
                             toEmail:(NSString*)toEmail
                      viewController:(UIViewController*)viewController
                             browser:(Browser*)browser {
  DCHECK(!_alertCoordinator);
  NSString* title = l10n_util::GetNSString(IDS_IOS_MANAGED_SWITCH_TITLE);
  NSString* subtitle = l10n_util::GetNSStringF(
      IDS_IOS_MANAGED_SWITCH_SUBTITLE, base::SysNSStringToUTF16(managedEmail),
      base::SysNSStringToUTF16(toEmail),
      base::SysNSStringToUTF16(hostedDomain));
  NSString* acceptLabel =
      l10n_util::GetNSString(IDS_IOS_MANAGED_SWITCH_ACCEPT_BUTTON);
  NSString* cancelLabel = l10n_util::GetNSString(IDS_CANCEL);

  _alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:viewController
                                                   browser:browser
                                                     title:title
                                                   message:subtitle];

  __weak AuthenticationFlowPerformer* weakSelf = self;
  __weak AlertCoordinator* weakAlert = _alertCoordinator;
  ProceduralBlock acceptBlock = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf alertControllerDidDisappear:weakAlert];
    [[strongSelf delegate]
        didChooseClearDataPolicy:SHOULD_CLEAR_DATA_CLEAR_DATA];
  };
  ProceduralBlock cancelBlock = ^{
    AuthenticationFlowPerformer* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf alertControllerDidDisappear:weakAlert];
    [[strongSelf delegate] didChooseCancel];
  };

  [_alertCoordinator addItemWithTitle:cancelLabel
                               action:cancelBlock
                                style:UIAlertActionStyleCancel];
  [_alertCoordinator addItemWithTitle:acceptLabel
                               action:acceptBlock
                                style:UIAlertActionStyleDefault];
  [_alertCoordinator setCancelAction:cancelBlock];
  [_alertCoordinator start];
}

// Callback for when the alert is dismissed.
- (void)alertControllerDidDisappear:(AlertCoordinator*)alertCoordinator {
  if (_alertCoordinator != alertCoordinator) {
    // Do not reset the `_alertCoordinator` if it has changed. This typically
    // happens when the user taps on any of the actions on "Clear Data Before
    // Syncing?" dialog, as the sign-in confirmation dialog is created before
    // the "Clear Data Before Syncing?" dialog is dismissed.
    return;
  }
  _alertCoordinator = nil;
}

@end
