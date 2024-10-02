// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"

#import <UIKit/UIKit.h>
#include <memory>

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_sync_controller_data_source.h"
#import "ios/web_view/public/cwv_sync_controller_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CWVSyncController ()

// Called by WebViewSyncControllerObserverBridge's |OnSyncShutdown|.
- (void)didShutdownSync;
// Called by WebViewSyncControllerObserverBridge's |OnStateChanged|.
- (void)syncStateDidChange;

@end

namespace ios_web_view {

// Bridge that observes syncer::SyncService and calls analagous
// methods on CWVSyncController.
class WebViewSyncControllerObserverBridge : public syncer::SyncServiceObserver {
 public:
  explicit WebViewSyncControllerObserverBridge(
      CWVSyncController* sync_controller)
      : sync_controller_(sync_controller) {}

  void OnStateChanged(syncer::SyncService* sync) override {
    [sync_controller_ syncStateDidChange];
  }

  void OnSyncShutdown(syncer::SyncService* sync) override {
    [sync_controller_ didShutdownSync];
  }

 private:
  __weak CWVSyncController* sync_controller_;
};

}  // namespace ios_web_view

@implementation CWVSyncController {
  syncer::SyncService* _syncService;
  signin::IdentityManager* _identityManager;
  std::unique_ptr<ios_web_view::WebViewSyncControllerObserverBridge> _observer;
  PrefService* _prefService;
  syncer::SyncService::TransportState _lastTransportState;
}

namespace {
// Provider of trusted vault features.
__weak id<CWVTrustedVaultProvider> gTrustedVaultProvider;
// Data source that can provide access tokens.
__weak id<CWVSyncControllerDataSource> gSyncDataSource;
}

+ (void)setTrustedVaultProvider:
    (id<CWVTrustedVaultProvider>)trustedVaultProvider {
  gTrustedVaultProvider = trustedVaultProvider;
}

+ (id<CWVTrustedVaultProvider>)trustedVaultProvider {
  return gTrustedVaultProvider;
}

+ (void)setDataSource:(id<CWVSyncControllerDataSource>)dataSource {
  gSyncDataSource = dataSource;
}

+ (id<CWVSyncControllerDataSource>)dataSource {
  return gSyncDataSource;
}

- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
                    identityManager:(signin::IdentityManager*)identityManager
                        prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _syncService = syncService;
    _identityManager = identityManager;
    _prefService = prefService;
    _observer =
        std::make_unique<ios_web_view::WebViewSyncControllerObserverBridge>(
            self);
    _syncService->AddObserver(_observer.get());
  }
  return self;
}

- (void)dealloc {
  _syncService->RemoveObserver(_observer.get());
}

#pragma mark - Public Methods

- (CWVIdentity*)currentIdentity {
  if (_identityManager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    CoreAccountInfo accountInfo =
        _identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
    return [[CWVIdentity alloc]
        initWithEmail:base::SysUTF8ToNSString(accountInfo.email)
             fullName:nil
               gaiaID:base::SysUTF8ToNSString(accountInfo.gaia)];
  }

  return nil;
}

- (BOOL)isPassphraseNeeded {
  return _syncService->GetUserSettings()
      ->IsPassphraseRequiredForPreferredDataTypes();
}

- (BOOL)isTrustedVaultKeysRequired {
  return _syncService->GetUserSettings()
      ->IsTrustedVaultKeyRequiredForPreferredDataTypes();
}

- (BOOL)isTrustedVaultRecoverabilityDegraded {
  return _syncService->GetUserSettings()
      ->IsTrustedVaultRecoverabilityDegraded();
}

- (void)startSyncWithIdentity:(CWVIdentity*)identity {
  DCHECK(!self.currentIdentity)
      << "Already syncing! Call -stopSyncAndClearIdentity first.";

  _identityManager->GetDeviceAccountsSynchronizer()
      ->ReloadAllAccountsFromSystemWithPrimaryAccount(CoreAccountId());

  const CoreAccountId accountId = _identityManager->PickAccountIdForAccount(
      base::SysNSStringToUTF8(identity.gaiaID),
      base::SysNSStringToUTF8(identity.email));
  CHECK(_identityManager->HasAccountWithRefreshToken(accountId));

  _identityManager->GetPrimaryAccountMutator()->SetPrimaryAccount(
      accountId, signin::ConsentLevel::kSync);
  CHECK_EQ(_identityManager->GetPrimaryAccountId(signin::ConsentLevel::kSync),
           accountId);

  autofill::prefs::SetUserOptedInWalletSyncTransport(_prefService, accountId,
                                                     /*opted_in=*/true);
  CHECK(password_manager::features_util::IsOptedInForAccountStorage(
      _prefService, _syncService));
}

- (void)stopSyncAndClearIdentity {
  auto* primaryAccountMutator = _identityManager->GetPrimaryAccountMutator();
  primaryAccountMutator->ClearPrimaryAccount(
      signin_metrics::ProfileSignout::kUserClickedSignoutSettings,
      signin_metrics::SignoutDelete::kIgnoreMetric);
}

- (BOOL)unlockWithPassphrase:(NSString*)passphrase {
  return _syncService->GetUserSettings()->SetDecryptionPassphrase(
      base::SysNSStringToUTF8(passphrase));
}

#pragma mark - Private Methods

- (void)didShutdownSync {
  _syncService->RemoveObserver(_observer.get());
}

- (void)syncStateDidChange {
  if (_lastTransportState != _syncService->GetTransportState()) {
    _lastTransportState = _syncService->GetTransportState();

    if (_lastTransportState == syncer::SyncService::TransportState::ACTIVE) {
      if ([_delegate
              respondsToSelector:@selector(syncControllerDidStartSync:)]) {
        [_delegate syncControllerDidStartSync:self];
      }
    } else if (_lastTransportState ==
               syncer::SyncService::TransportState::DISABLED) {
      if ([_delegate
              respondsToSelector:@selector(syncControllerDidStopSync:)]) {
        [_delegate syncControllerDidStopSync:self];
      }
    }
  }

  if ([_delegate respondsToSelector:@selector(syncControllerDidUpdateState:)]) {
    [_delegate syncControllerDidUpdateState:self];
  }
}

@end
