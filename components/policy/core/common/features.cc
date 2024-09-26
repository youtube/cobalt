// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/features.h"

#include "google_apis/gaia/gaia_constants.h"

namespace policy {

namespace features {

BASE_FEATURE(kLoginEventReporting,
             "LoginEventReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPasswordBreachEventReporting,
             "PasswordBreachEventReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableUserCloudSigninRestrictionPolicyFetcher,
             "UserCloudSigninRestrictionPolicyFetcher",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDmTokenDeletion,
             "DmTokenDeletion",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kListPoliciesAcceptCommaSeparatedStringsAndroid,
             "ListPoliciesAcceptCommaSeparatedStringsAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPolicyLogsPageAndroid,
             "PolicyLogsPageAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSafeSitesFilterBehaviorPolicyAndroid,
             "SafeSitesFilterBehaviorPolicyAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kPolicyMergeMultiSource,
             "PolicyMergeMultiSource",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kPolicyLogsPageIOS,
             "PolicyLogsPageIOS",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_IOS)

}  // namespace features

}  // namespace policy
