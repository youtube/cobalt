// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_FEATURES_H_
#define COMPONENTS_TRUSTED_VAULT_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace trusted_vault {

// Whether the periodic degraded recoverability polling is enabled.
BASE_DECLARE_FEATURE(kSyncTrustedVaultPeriodicDegradedRecoverabilityPolling);
inline constexpr base::FeatureParam<base::TimeDelta>
    kSyncTrustedVaultLongPeriodDegradedRecoverabilityPolling{
        &kSyncTrustedVaultPeriodicDegradedRecoverabilityPolling,
        "kSyncTrustedVaultLongPeriodDegradedRecoverabilityPolling",
        base::Days(7)};
inline constexpr base::FeatureParam<base::TimeDelta>
    kSyncTrustedVaultShortPeriodDegradedRecoverabilityPolling{
        &kSyncTrustedVaultPeriodicDegradedRecoverabilityPolling,
        "kSyncTrustedVaultShortPeriodDegradedRecoverabilityPolling",
        base::Hours(1)};

// Enables logging a UMA metric that requires first communicating with the
// trusted vault server, in order to verify that the local notion of the device
// being registered is consistent with the server-side state.
BASE_DECLARE_FEATURE(kSyncTrustedVaultVerifyDeviceRegistration);

// If enabled, degraded recoverability is polled once per minute. This overrides
// polling period params above. Useful for manual testing.
BASE_DECLARE_FEATURE(kTrustedVaultFrequentDegradedRecoverabilityPolling);

#if !BUILDFLAG(IS_ANDROID)
// Enables the chrome.setClientEncryptionKeys() JS API.
BASE_DECLARE_FEATURE(kSetClientEncryptionKeysJsApi);
#endif

#if BUILDFLAG(IS_CHROMEOS)
// If enabled - trusted vault error pages will be opened in WebUI dialog instead
// of browser tab. In Ash this behavioral change additionally guarded by
// LacrosOnly mode.
BASE_DECLARE_FEATURE(kChromeOSTrustedVaultUseWebUIDialog);

// Enables sharing of TrustedVaultClient between Ash and Lacros main profile via
// Crosapi: Ash will keep a stateful TrustedVaultClient and Lacros will use it
// via IPC to implement stateless TrustedVaultClient.
BASE_DECLARE_FEATURE(kChromeOSTrustedVaultClientShared);

#endif

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_FEATURES_H_
