// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_mediator.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_consumer.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The label the bottom sheet should display, or nil if there should be none.
// The label should never promise "sign in to achieve X" if an enterprise
// policy is preventing X, thus the 2 parameters:
//   `sync_transport_disabled_by_policy`: Whether the sync-transport layer got
//   completely nuked by the SyncDisabled policy.
//   `sync_types_disabled_by_policy`: Any syncer::UserSelectableTypes disabled
//   via the SyncTypesListDisabled policy.
// Note: `sync_transport_disabled_by_policy` true is a different product state
// from `sync_types_disabled_by_policy` containing all controllable types,
// because some features are not gated behind a user-controllable toggle, e.g.
// send-tab-to-self. That's why both parameters are required.
NSString* GetPromoLabelString(
    signin_metrics::AccessPoint access_point,
    bool sync_transport_disabled_by_policy,
    syncer::UserSelectableTypeSet sync_types_disabled_by_policy) {
  switch (access_point) {
    case signin_metrics::AccessPoint::kSendTabToSelfPromo:
      // Sign-in shouldn't be offered if the feature doesn't work.
      CHECK(!sync_transport_disabled_by_policy);
      return l10n_util::GetNSString(IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_LABEL);
    case signin_metrics::AccessPoint::kNtpFeedCardMenuPromo:
      // Configuring feed interests is independent of sync.
      return l10n_util::GetNSString(IDS_IOS_FEED_CARD_SIGN_IN_ONLY_PROMO_LABEL);
    case signin_metrics::AccessPoint::kWebSignin:
      // This could check `sync_types_disabled_by_policy` only for the types
      // mentioned in the regular string, but don't bother.
      return sync_transport_disabled_by_policy ||
                     !sync_types_disabled_by_policy.empty()
                 ? l10n_util::GetNSString(
                       IDS_IOS_CONSISTENCY_PROMO_DEFAULT_ACCOUNT_LABEL)
                 : l10n_util::GetNSString(
                       IDS_IOS_SIGNIN_SHEET_LABEL_FOR_WEB_SIGNIN);
    case signin_metrics::AccessPoint::kNtpSignedOutIcon:
      // This could check `sync_types_disabled_by_policy` only for the types
      // mentioned in the regular string, but don't bother.
      return sync_transport_disabled_by_policy ||
                     !sync_types_disabled_by_policy.empty()
                 ? nil
                 : l10n_util::GetNSString(
                       IDS_IOS_IDENTITY_DISC_SIGN_IN_PROMO_LABEL);
    case signin_metrics::AccessPoint::kSetUpList:
      // "Sync" is mentioned in the setup list (the card, not this sheet). So it
      // was easier to hide it than come up with new strings. In the future, we
      // could tweak the card strings and return nil here.
      CHECK(!sync_transport_disabled_by_policy &&
            sync_types_disabled_by_policy.empty());
      return l10n_util::GetNSString(IDS_IOS_IDENTITY_DISC_SIGN_IN_PROMO_LABEL);
    case signin_metrics::AccessPoint::kNtpFeedTopPromo:
    case signin_metrics::AccessPoint::kNtpFeedBottomPromo:
      // Feed personalization is independent of sync.
      return l10n_util::GetNSString(IDS_IOS_SIGNIN_SHEET_LABEL_FOR_FEED_PROMO);
    case signin_metrics::AccessPoint::kRecentTabs:
      // Sign-in shouldn't be offered if the feature doesn't work.
      CHECK(!sync_transport_disabled_by_policy &&
            !sync_types_disabled_by_policy.Has(
                syncer::UserSelectableType::kTabs));
      return l10n_util::GetNSString(IDS_IOS_SIGNIN_SHEET_LABEL_FOR_RECENT_TABS);
    case signin_metrics::AccessPoint::kNotificationsOptInScreenContentToggle:
      return l10n_util::GetNSString(
          IDS_IOS_NOTIFICATIONS_OPT_IN_SIGN_IN_MESSAGE_CONTENT);
    case signin_metrics::AccessPoint::kSettings:
      // No text.
      return nil;
    case signin_metrics::AccessPoint::kStartPage:
    case signin_metrics::AccessPoint::kNtpLink:
    case signin_metrics::AccessPoint::kMenu:
    case signin_metrics::AccessPoint::kSupervisedUser:
    case signin_metrics::AccessPoint::kExtensionInstallBubble:
    case signin_metrics::AccessPoint::kExtensions:
    case signin_metrics::AccessPoint::kBookmarkBubble:
    case signin_metrics::AccessPoint::kBookmarkManager:
    case signin_metrics::AccessPoint::kAvatarBubbleSignIn:
    case signin_metrics::AccessPoint::kUserManager:
    case signin_metrics::AccessPoint::kDevicesPage:
    case signin_metrics::AccessPoint::kSigninPromo:
    case signin_metrics::AccessPoint::kUnknown:
    case signin_metrics::AccessPoint::kPasswordBubble:
    case signin_metrics::AccessPoint::kAutofillDropdown:
    case signin_metrics::AccessPoint::kResigninInfobar:
    case signin_metrics::AccessPoint::kTabSwitcher:
    case signin_metrics::AccessPoint::kMachineLogon:
    case signin_metrics::AccessPoint::kGoogleServicesSettings:
    case signin_metrics::AccessPoint::kSyncErrorCard:
    case signin_metrics::AccessPoint::kForcedSignin:
    case signin_metrics::AccessPoint::kAccountRenamed:
    case signin_metrics::AccessPoint::kSafetyCheck:
    case signin_metrics::AccessPoint::kKaleidoscope:
    case signin_metrics::AccessPoint::kEnterpriseSignoutCoordinator:
    case signin_metrics::AccessPoint::kSigninInterceptFirstRunExperience:
    case signin_metrics::AccessPoint::kSettingsSyncOffRow:
    case signin_metrics::AccessPoint::kPostDeviceRestoreSigninPromo:
    case signin_metrics::AccessPoint::kPostDeviceRestoreBackgroundSignin:
    case signin_metrics::AccessPoint::kDesktopSigninManager:
    case signin_metrics::AccessPoint::kForYouFre:
    case signin_metrics::AccessPoint::kCreatorFeedFollow:
    case signin_metrics::AccessPoint::kReadingList:
    case signin_metrics::AccessPoint::kReauthInfoBar:
    case signin_metrics::AccessPoint::kAccountConsistencyService:
    case signin_metrics::AccessPoint::kSearchCompanion:
    case signin_metrics::AccessPoint::kPasswordMigrationWarningAndroid:
    case signin_metrics::AccessPoint::kSaveToDriveIos:
    case signin_metrics::AccessPoint::kSaveToPhotosIos:
    case signin_metrics::AccessPoint::kChromeSigninInterceptBubble:
    case signin_metrics::AccessPoint::kRestorePrimaryAccountOnProfileLoad:
    case signin_metrics::AccessPoint::kTabOrganization:
    case signin_metrics::AccessPoint::kTipsNotification:
    case signin_metrics::AccessPoint::kSigninChoiceRemembered:
    case signin_metrics::AccessPoint::kProfileMenuSignoutConfirmationPrompt:
    case signin_metrics::AccessPoint::kSettingsSignoutConfirmationPrompt:
    case signin_metrics::AccessPoint::kNtpIdentityDisc:
    case signin_metrics::AccessPoint::kOidcRedirectionInterception:
    case signin_metrics::AccessPoint::kWebauthnModalDialog:
    case signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo:
    case signin_metrics::AccessPoint::kProductSpecifications:
    case signin_metrics::AccessPoint::kAccountMenu:
    case signin_metrics::AccessPoint::kAccountMenuFailedSwitch:
    case signin_metrics::AccessPoint::kAddressBubble:
    case signin_metrics::AccessPoint::kCctAccountMismatchNotification:
    case signin_metrics::AccessPoint::kDriveFilePickerIos:
    case signin_metrics::AccessPoint::kCollaborationTabGroup:
    case signin_metrics::AccessPoint::kGlicLaunchButton:
    case signin_metrics::AccessPoint::kHistoryPage:
      // Nothing prevents instantiating ConsistencyDefaultAccountViewController
      // with an arbitrary entry point, API-wise. In doubt, no label is a good,
      // generic default that fits all entry points.
      return nil;
  }
}

}  // namespace

@interface ConsistencyDefaultAccountMediator () <
    ChromeAccountManagerServiceObserver,
    IdentityManagerObserverBridgeDelegate> {
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  signin_metrics::AccessPoint _accessPoint;
  raw_ptr<syncer::SyncService> _syncService;
}

@property(nonatomic, strong) UIImage* avatar;

@end

@implementation ConsistencyDefaultAccountMediator {
  raw_ptr<signin::IdentityManager> _identityManager;
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
}

- (instancetype)
    initWithIdentityManager:(signin::IdentityManager*)identityManager
      accountManagerService:(ChromeAccountManagerService*)accountManagerService
                syncService:(syncer::SyncService*)syncService
                accessPoint:(signin_metrics::AccessPoint)accessPoint {
  if ((self = [super init])) {
    CHECK(identityManager);
    CHECK(accountManagerService);
    CHECK(syncService);

    _identityManager = identityManager;
    _accountManagerService = accountManagerService;
    _syncService = syncService;
    _accessPoint = accessPoint;

    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_identityManager);
  DCHECK(!_accountManagerService);
  DCHECK(!_syncService);
}

- (void)disconnect {
  _identityManager = nullptr;
  _accountManagerService = nullptr;
  _syncService = nullptr;
  _identityManagerObserver.reset();
  _accountManagerServiceObserver.reset();
}

#pragma mark - Properties

- (void)setConsumer:(id<ConsistencyDefaultAccountConsumer>)consumer {
  CHECK(_syncService);

  _consumer = consumer;

  syncer::UserSelectableTypeSet disabledTypes;
  syncer::SyncUserSettings* syncSettings = _syncService->GetUserSettings();
  for (syncer::UserSelectableType type :
       syncSettings->GetRegisteredSelectableTypes()) {
    if (syncSettings->IsTypeManagedByPolicy(type)) {
      disabledTypes.Put(type);
    }
  }
  NSString* labelText = GetPromoLabelString(
      _accessPoint,
      _syncService->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY),
      disabledTypes);
  [_consumer setLabelText:labelText];

  NSString* skipButtonText =
      _accessPoint == signin_metrics::AccessPoint::kWebSignin
          ? l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_SKIP)
          : l10n_util::GetNSString(IDS_CANCEL);
  [_consumer setSkipButtonText:skipButtonText];

  [self selectSelectedIdentity];
}

- (void)setSelectedIdentity:(id<SystemIdentity>)identity {
  if ([_selectedIdentity isEqual:identity]) {
    return;
  }
  _selectedIdentity = identity;
  [self updateSelectedIdentityUI];
}

#pragma mark - Private

// Updates the default identity, or hide the default identity if there isn't
// one present on the device.
- (void)selectSelectedIdentity {
  if (!_identityManager || !_accountManagerService) {
    return;
  }

  // Here, default identity may be nil.
  self.selectedIdentity = signin::GetDefaultIdentityOnDevice(
      _identityManager, _accountManagerService);
}

// Updates the view controller using the default identity, or hide the default
// identity button if no identity is present on device.
- (void)updateSelectedIdentityUI {
  if (!self.selectedIdentity) {
    [self.consumer hideDefaultAccount];
    return;
  }

  id<SystemIdentity> selectedIdentity = self.selectedIdentity;
  UIImage* avatar = _accountManagerService->GetIdentityAvatarWithIdentity(
      selectedIdentity, IdentityAvatarSize::TableViewIcon);
  BOOL isManaged = [self isIdentityKnownToBeManaged:selectedIdentity];
  [self.consumer showDefaultAccountWithFullName:selectedIdentity.userFullName
                                      givenName:selectedIdentity.userGivenName
                                          email:selectedIdentity.userEmail
                                         avatar:avatar
                                        managed:isManaged];
}

- (void)handleIdentityListChanged {
  [self selectSelectedIdentity];
}

- (void)handleIdentityUpdated:(id<SystemIdentity>)identity {
  if ([self.selectedIdentity isEqual:identity]) {
    [self updateSelectedIdentityUI];
  }
}

// Returns true if `identity` is known to be managed.
// Returns false if the identity is known not to be managed or if the management
// status is unknown. If the management status is unknown, it is fetched by
// calling `FetchManagedStatusForIdentity`. `handleIdentityUpdated:` will be
// called asynchronously when the management status if retrieved and the
// identity is managed.
- (BOOL)isIdentityKnownToBeManaged:(id<SystemIdentity>)identity {
  if (std::optional<BOOL> managed = IsIdentityManaged(identity);
      managed.has_value()) {
    return managed.value();
  }

  __weak __typeof(self) weakSelf = self;
  FetchManagedStatusForIdentity(identity, base::BindOnce(^(bool managed) {
                                  if (managed) {
                                    [weakSelf handleIdentityUpdated:identity];
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
