// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_trusted_vault_utils.h"

#import "components/trusted_vault/trusted_vault_histograms.h"

namespace {
trusted_vault::TrustedVaultDeviceRegistrationStateForUMA
CWVConvertTrustedVaultState(CWVTrustedVaultState state) {
  switch (state) {
    case CWVTrustedVaultStateAlreadyRegisteredV0:
      return trusted_vault::TrustedVaultDeviceRegistrationStateForUMA::
          kAlreadyRegisteredV0;
    case CWVTrustedVaultStateLocalKeysAreStale:
      return trusted_vault::TrustedVaultDeviceRegistrationStateForUMA::
          kLocalKeysAreStale;
    case CWVTrustedVaultStateThrottledClientSide:
      return trusted_vault::TrustedVaultDeviceRegistrationStateForUMA::
          kThrottledClientSide;
    case CWVTrustedVaultStateAttemptingRegistrationWithNewKeyPair:
      return trusted_vault::TrustedVaultDeviceRegistrationStateForUMA::
          kAttemptingRegistrationWithNewKeyPair;
    case CWVTrustedVaultStateAttemptingRegistrationWithExistingKeyPair:
      return trusted_vault::TrustedVaultDeviceRegistrationStateForUMA::
          kAttemptingRegistrationWithExistingKeyPair;
    case CWVTrustedVaultStateAttemptingRegistrationWithPersistentAuthError:
      // TODO(crbug.com/1418027): remove CWV version of this bucket.
      return trusted_vault::TrustedVaultDeviceRegistrationStateForUMA::
          kDeprecatedAttemptingRegistrationWithPersistentAuthError;
    case CWVTrustedVaultStateAlreadyRegisteredV1:
      return trusted_vault::TrustedVaultDeviceRegistrationStateForUMA::
          kAlreadyRegisteredV1;
  }
}
}  // namespace

@implementation CWVTrustedVaultUtils

+ (void)logTrustedVaultDidUpdateState:(CWVTrustedVaultState)state {
  trusted_vault::RecordTrustedVaultDeviceRegistrationState(
      CWVConvertTrustedVaultState(state));
}

+ (void)logTrustedVaultDidReceiveHTTPStatusCode:(NSInteger)statusCode {
  trusted_vault::RecordTrustedVaultURLFetchResponse(
      statusCode, /*net_error=*/0,
      trusted_vault::TrustedVaultURLFetchReasonForUMA::kUnspecified);
}

+ (void)logTrustedVaultDidFailKeyDistribution:(NSError*)error {
  // TODO(crbug.com/1266130): Check to see if any UMA logging needs to occur.
}

@end
