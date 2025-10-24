// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace trusted_vault {

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSetClientEncryptionKeysJsApi,
             "SetClientEncryptionKeysJsApi",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kEnableICloudKeychainRecoveryFactor,
             "EnableICloudKeychainRecoveryFactor",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kEnableRegistrationStateSecurityDomainFiltering,
             "EnableRegistrationStateSecurityDomainFiltering",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace trusted_vault
