// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/signin/signin_screen_mediator.h"

#import "base/containers/contains.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/service/sync_service.h"
#import "components/web_resource/web_resource_pref_names.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/logging/first_run_signin_logger.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/logging/user_signin_logger.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/first_run/ui_bundled/signin/signin_screen_consumer.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/enterprise_utils.h"

namespace {
// Represents the state of the screen relevant to the First Run.
enum class SigninScreenState {
  // The screen is the first screen in the FRE sequence.
  kFirstRunAsFirstScreen,
  // The screen is in the FRE sequence, but is not the first screen.
  kFirstRunAsOtherScreen,
  // The screen is not in the FRE.
  kNotFirstRun,
};
}  // namespace

@interface SigninScreenMediator () <ChromeAccountManagerServiceObserver,
                                    IdentityManagerObserverBridgeDelegate> {
}

// Application local pref.
@property(nonatomic, assign) PrefService* localPrefService;
// User pref.
@property(nonatomic, assign) PrefService* prefService;
// Sync service.
@property(nonatomic, assign) syncer::SyncService* syncService;
// Logger used to record sign in metrics.
@property(nonatomic, strong) UserSigninLogger* logger;
// Whether the user attempted to sign in (the attempt can be successful, failed
// or canceled).
@property(nonatomic, assign, readwrite)
    first_run::SignInAttemptStatus attemptStatus;
// Whether there was existing accounts when the screen was presented.
@property(nonatomic, assign) BOOL hadIdentitiesAtStartup;

@end

@implementation SigninScreenMediator {
  // Account manager service to retrieve Chrome identities.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  // Authentication service for sign in.
  raw_ptr<AuthenticationService> _authenticationService;
  // Identity manager to retrieve Chrome identities.
  raw_ptr<signin::IdentityManager> _identityManager;
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  // State of the sign-in screen.
  SigninScreenState _screenState;
}

- (instancetype)
    initWithAccountManagerService:
        (ChromeAccountManagerService*)accountManagerService
            authenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager
                 localPrefService:(PrefService*)localPrefService
                      prefService:(PrefService*)prefService
                      syncService:(syncer::SyncService*)syncService
                      accessPoint:(signin_metrics::AccessPoint)accessPoint
                      promoAction:(signin_metrics::PromoAction)promoAction {
  self = [super init];
  if (self) {
    CHECK(accountManagerService);
    CHECK(authenticationService);
    CHECK(identityManager);
    CHECK(localPrefService);
    CHECK(prefService);
    CHECK(syncService);

    _UMAReportingUserChoice = kDefaultMetricsReportingCheckboxValue;
    _accountManagerService = accountManagerService;
    _authenticationService = authenticationService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);
    _localPrefService = localPrefService;
    _prefService = prefService;
    _syncService = syncService;

    if (IsUseAccountListFromIdentityManagerEnabled()) {
      _hadIdentitiesAtStartup =
          !_identityManager->GetAccountsOnDevice().empty();
    } else {
      _hadIdentitiesAtStartup = _accountManagerService->HasIdentities();
    }

    if (accessPoint == signin_metrics::AccessPoint::kStartPage) {
      if (!_localPrefService->GetBoolean(prefs::kEulaAccepted)) {
        _screenState = SigninScreenState::kFirstRunAsFirstScreen;
      } else {
        _screenState = SigninScreenState::kFirstRunAsOtherScreen;
      }
    } else {
      _screenState = SigninScreenState::kNotFirstRun;
    }

    switch (_screenState) {
      case SigninScreenState::kNotFirstRun:
        _logger = [[UserSigninLogger alloc] initWithAccessPoint:accessPoint
                                                    promoAction:promoAction];
        break;
      case SigninScreenState::kFirstRunAsFirstScreen:
      case SigninScreenState::kFirstRunAsOtherScreen:
        _logger =
            [[FirstRunSigninLogger alloc] initWithAccessPoint:accessPoint
                                                  promoAction:promoAction];
        break;
    }

    _ignoreDismissGesture =
        accessPoint == signin_metrics::AccessPoint::kStartPage ||
        accessPoint == signin_metrics::AccessPoint::kForcedSignin;

    [_logger logSigninStarted];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_accountManagerService);
  DCHECK(!_authenticationService);
  DCHECK(!_identityManager);
  DCHECK(!self.localPrefService);
  DCHECK(!self.prefService);
  DCHECK(!self.syncService);
}

- (void)disconnect {
  _accountManagerService = nullptr;
  _authenticationService = nullptr;
  _identityManager = nullptr;
  self.localPrefService = nullptr;
  self.prefService = nullptr;
  self.syncService = nullptr;
  _accountManagerServiceObserver.reset();
  _identityManagerObserver.reset();
}

- (void)startSignInWithAuthenticationFlow:
            (AuthenticationFlow*)authenticationFlow
                               completion:(ProceduralBlock)completion {
  [self userAttemptedToSignin];
  RecordMetricsReportingDefaultState();

  // The sign-in screen should not be displayed if the user is already
  // signed-in.
  CHECK(!_authenticationService->HasPrimaryIdentity(
            signin::ConsentLevel::kSignin),
        base::NotFatalUntil::M140);
  [self.consumer setUIEnabled:NO];
  __weak __typeof(self) weakSelf = self;
  [authenticationFlow startSignInWithCompletion:^(
                          SigninCoordinatorResult result) {
    [weakSelf.consumer setUIEnabled:YES];
    if (result != SigninCoordinatorResultSuccess) {
      return;
    }
    [weakSelf.logger logSigninCompletedWithResult:SigninCoordinatorResultSuccess
                                     addedAccount:weakSelf.addedAccount];
    if (completion) {
      completion();
    }
  }];
}

- (void)cancelSignInScreenWithCompletion:(ProceduralBlock)completion {
  // The sign-in screen should not be displayed if the user is already
  // signed-in.
  CHECK(!_authenticationService->HasPrimaryIdentity(
            signin::ConsentLevel::kSignin),
        base::NotFatalUntil::M140);
  if (completion) {
    completion();
  }
}

- (void)userAttemptedToSignin {
  DCHECK_NE(self.attemptStatus,
            first_run::SignInAttemptStatus::SKIPPED_BY_POLICY);
  DCHECK_NE(self.attemptStatus, first_run::SignInAttemptStatus::NOT_SUPPORTED);
  self.attemptStatus = first_run::SignInAttemptStatus::ATTEMPTED;
}

- (void)finishPresentingWithSignIn:(BOOL)signIn {
  if (self.TOSLinkWasTapped) {
    base::RecordAction(base::UserMetricsAction("MobileFreTOSLinkTapped"));
  }
  if (self.UMALinkWasTapped) {
    base::RecordAction(base::UserMetricsAction("MobileFreUMALinkTapped"));
  }
  if (_screenState != SigninScreenState::kNotFirstRun) {
    first_run::FirstRunStage firstRunStage =
        signIn ? first_run::kWelcomeAndSigninScreenCompletionWithSignIn
               : first_run::kWelcomeAndSigninScreenCompletionWithoutSignIn;
    base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                  firstRunStage);
    RecordFirstRunSignInMetrics(_identityManager, self.attemptStatus,
                                self.hadIdentitiesAtStartup);
  }
  if (_screenState == SigninScreenState::kFirstRunAsFirstScreen) {
    self.localPrefService->SetBoolean(prefs::kEulaAccepted, true);
    self.localPrefService->SetBoolean(metrics::prefs::kMetricsReportingEnabled,
                                      self.UMAReportingUserChoice);
    self.localPrefService->CommitPendingWrite();
  }
}

#pragma mark - Properties

- (void)setConsumer:(id<SigninScreenConsumer>)consumer {
  DCHECK(consumer);
  DCHECK(!_consumer);
  _consumer = consumer;
  BOOL signinForcedOrAvailable = NO;
  switch (_authenticationService->GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
      self.attemptStatus = first_run::SignInAttemptStatus::NOT_ATTEMPTED;
      signinForcedOrAvailable = YES;
      _consumer.signinStatus = SigninScreenConsumerSigninStatusForced;
      break;
    case AuthenticationService::ServiceStatus::SigninAllowed:
      self.attemptStatus = first_run::SignInAttemptStatus::NOT_ATTEMPTED;
      signinForcedOrAvailable = YES;
      _consumer.signinStatus = SigninScreenConsumerSigninStatusAvailable;
      break;
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
      // This is possible only if FirstRun is triggered by the preferences to
      // test FRE.
      self.attemptStatus = first_run::SignInAttemptStatus::NOT_ATTEMPTED;
      _consumer.signinStatus = SigninScreenConsumerSigninStatusDisabled;
      break;
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
      self.attemptStatus = first_run::SignInAttemptStatus::SKIPPED_BY_POLICY;
      _consumer.signinStatus = SigninScreenConsumerSigninStatusDisabled;
      break;
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      self.attemptStatus = first_run::SignInAttemptStatus::NOT_SUPPORTED;
      _consumer.signinStatus = SigninScreenConsumerSigninStatusDisabled;
      break;
  }
  _consumer.syncEnabled =
      !_syncService->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) &&
      !HasManagedSyncDataType(_syncService);
  self.consumer.hasPlatformPolicies = HasPlatformPolicies();

  switch (_screenState) {
    case SigninScreenState::kNotFirstRun:
    case SigninScreenState::kFirstRunAsOtherScreen:
      self.consumer.screenIntent = SigninScreenConsumerScreenIntentSigninOnly;
      break;
    case SigninScreenState::kFirstRunAsFirstScreen:
      BOOL metricReportingDisabled =
          self.localPrefService->IsManagedPreference(
              metrics::prefs::kMetricsReportingEnabled) &&
          !self.localPrefService->GetBoolean(
              metrics::prefs::kMetricsReportingEnabled);
      self.consumer.screenIntent =
          metricReportingDisabled
              ? SigninScreenConsumerScreenIntentWelcomeWithoutUMAAndSignin
              : SigninScreenConsumerScreenIntentWelcomeAndSignin;
      break;
  }

  if (signinForcedOrAvailable) {
    self.selectedIdentity = signin::GetDefaultIdentityOnDevice(
        _identityManager, _accountManagerService);
  }
}

- (void)setSelectedIdentity:(id<SystemIdentity>)selectedIdentity {
  if ([_selectedIdentity isEqual:selectedIdentity]) {
    return;
  }
  // nil is allowed only if there is no other identity.
  if (IsUseAccountListFromIdentityManagerEnabled()) {
    DCHECK(selectedIdentity || _identityManager->GetAccountsOnDevice().empty());
  } else {
    DCHECK(selectedIdentity || !_accountManagerService->HasIdentities());
  }
  _selectedIdentity = selectedIdentity;

  [self updateConsumerIdentity];
}

#pragma mark - Private

- (bool)selectedIdentityIsValid {
  if (IsUseAccountListFromIdentityManagerEnabled()) {
    if (self.selectedIdentity) {
      GaiaId gaia(self.selectedIdentity.gaiaID);
      return base::Contains(_identityManager->GetAccountsOnDevice(), gaia,
                            [](const AccountInfo& info) { return info.gaia; });
    }
    return false;
  } else {
    return _accountManagerService->IsValidIdentity(self.selectedIdentity);
  }
}

- (void)updateConsumerIdentity {
  switch (_authenticationService->GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed:
      break;
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      return;
  }
  id<SystemIdentity> selectedIdentity = self.selectedIdentity;
  if (!selectedIdentity) {
    [self.consumer noIdentityAvailable];
  } else {
    UIImage* avatar = _accountManagerService->GetIdentityAvatarWithIdentity(
        selectedIdentity, IdentityAvatarSize::Regular);

    [self.consumer setSelectedIdentityUserName:selectedIdentity.userFullName
                                         email:selectedIdentity.userEmail
                                     givenName:selectedIdentity.userGivenName
                                        avatar:avatar
                                       managed:[self isIdentityKnownToBeManaged:
                                                         selectedIdentity]];
  }
}

- (void)handleIdentityListChanged {
  if (![self selectedIdentityIsValid]) {
    self.selectedIdentity = signin::GetDefaultIdentityOnDevice(
        _identityManager, _accountManagerService);
  }
}

- (void)handleIdentityUpdated:(id<SystemIdentity>)identity {
  if ([self.selectedIdentity isEqual:identity]) {
    [self updateConsumerIdentity];
  }
}

// Returns true if `identity` is known to be managed.
// Returns false if the identity is known not to be managed or if the management
// status is unknown. If the management status is unknown, it is fetched by
// calling `FetchManagedStatusForIdentity`. `identityUpdated:` will be called
// asynchronously when the management status if retrieved and the identity is
// managed.
- (BOOL)isIdentityKnownToBeManaged:(id<SystemIdentity>)identity {
  if (std::optional<BOOL> managed = IsIdentityManaged(identity);
      managed.has_value()) {
    return managed.value();
  }

  __weak __typeof(self) weakSelf = self;
  FetchManagedStatusForIdentity(identity, base::BindOnce(^(bool managed) {
                                  if (managed) {
                                    [weakSelf identityUpdated:identity];
                                  }
                                }));
  return NO;
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  if (IsUseAccountListFromIdentityManagerEnabled()) {
    // Listening to `onAccountsOnDeviceChanged` instead.
    return;
  }
  [self handleIdentityListChanged];
}

- (void)identityUpdated:(id<SystemIdentity>)identity {
  if (IsUseAccountListFromIdentityManagerEnabled()) {
    // Listening to `onExtendedAccountInfoUpdated` instead.
    return;
  }
  [self handleIdentityUpdated:identity];
}

- (void)onChromeAccountManagerServiceShutdown:
    (ChromeAccountManagerService*)accountManagerService {
  // TODO(crbug.com/40284086): Remove `[self disconnect]`.
  [self disconnect];
}

#pragma mark -  IdentityManagerObserver

- (void)onAccountsOnDeviceChanged {
  if (!IsUseAccountListFromIdentityManagerEnabled()) {
    // Listening to `identityListChanged` instead.
    return;
  }
  [self handleIdentityListChanged];
}

- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)info {
  if (!IsUseAccountListFromIdentityManagerEnabled()) {
    // Listening to `identityUpdated` instead.
    return;
  }
  id<SystemIdentity> identity =
      _accountManagerService->GetIdentityOnDeviceWithGaiaID(info.gaia);
  [self handleIdentityUpdated:identity];
}
@end
