// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/common/features.h"

#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"

namespace supervised_user {

// Enables local parent approvals for the blocked website on the Family Link
// user's device.
BASE_FEATURE(kLocalWebApprovals,
             "LocalWebApprovals",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// TODO(crbug.com/391799078): Support local web approval for subframes on
// Desktop.
BASE_FEATURE(kAllowSubframeLocalWebApprovals,
             "AllowSubframeLocalWebApprovals",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_WIN)
const int kLocalWebApprovalBottomSheetLoadTimeoutDefaultValueMs = 5000;

const base::FeatureParam<int> kLocalWebApprovalBottomSheetLoadTimeoutMs{
    &kLocalWebApprovals, /*name=*/"LocalWebApprovalBottomSheetLoadTimeoutMs",
    kLocalWebApprovalBottomSheetLoadTimeoutDefaultValueMs};
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_WIN)

BASE_FEATURE(kLocalWebApprovalsWidgetSupportsUrlPayload,
             "PacpWidgetSupportsUrlPayload",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsGoogleBrandedBuild() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return false;
#endif
}

bool IsLocalWebApprovalsEnabled() {
  // TODO(crbug.com/1272462, b/261729051):
  // Move this logic to SupervisedUserService, once it's migrated to
  // components, and de-release the intended usage of
  // WebsiteParentApproval::IsLocalApprovalSupported for Android.
#if BUILDFLAG(IS_ANDROID)
  return base::FeatureList::IsEnabled(kLocalWebApprovals) &&
         IsGoogleBrandedBuild();
#else
  return base::FeatureList::IsEnabled(kLocalWebApprovals);
#endif
}

bool IsLocalWebApprovalsEnabledForSubframes() {
  return base::FeatureList::IsEnabled(kAllowSubframeLocalWebApprovals);
}

BASE_FEATURE(kEnableSupervisedUserSkipParentApprovalToInstallExtensions,
             "EnableSupervisedUserSkipParentApprovalToInstallExtensions",
             base::FEATURE_ENABLED_BY_DEFAULT
);

BASE_FEATURE(kUpdatedSupervisedUserExtensionApprovalStrings,
             "UpdatedSupervisedUserExtensionApprovalStrings",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
BASE_FEATURE(kEnableExtensionsPermissionsForSupervisedUsersOnDesktop,
             "EnableExtensionsPermissionsForSupervisedUsersOnDesktop",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
BASE_FEATURE(kExposedParentalControlNeededForExtensionInstallation,
             "ExposedParentalControlNeededForExtensionInstallation",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsSupervisedUserSkipParentApprovalToInstallExtensionsEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
  return base::FeatureList::IsEnabled(
      kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  bool skipParentApprovalEnabled = base::FeatureList::IsEnabled(
      kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
  bool permissionExtensionsForSupervisedUsersEnabled =
      base::FeatureList::IsEnabled(
          kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
  if (skipParentApprovalEnabled) {
    DCHECK(permissionExtensionsForSupervisedUsersEnabled);
  }
  return skipParentApprovalEnabled &&
         permissionExtensionsForSupervisedUsersEnabled;
#else
  NOTREACHED();
#endif  // BUILDFLAG(IS_CHROMEOS)
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
BASE_FEATURE(kCustomProfileStringsForSupervisedUsers,
             "CustomProfileStringsForSupervisedUsers",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kShowKiteForSupervisedUsers,
             "ShowKiteForSupervisedUsers",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kForceSafeSearchForUnauthenticatedSupervisedUsers,
             "ForceSafeSearchForUnauthenticatedSupervisedUsers",
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
BASE_FEATURE(kEnableSupervisedUserVersionSignOutDialog,
             "EnableSupervisedUserVersionSignOutDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kForceSupervisedUserReauthenticationForYouTube,
             "ForceSupervisedUserReauthenticationForYouTube",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/378636321): Clean-up this flag once
// `ForceSupervisedUserReauthenticationForYouTube` is enabled.
BASE_FEATURE(kExemptYouTubeInfrastructureFromBlocking,
             "ExemptYouTubeInfrastructureFromBlocking",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// TODO: crbug.com/378636321 - Clean up the
// kUncredentialedFilteringFallbackForSupervisedUsers and
// kWaitUntilAccessTokenAvailableForClassifyUrl flags, by inlining the
// platform #defines.
BASE_FEATURE(kUncredentialedFilteringFallbackForSupervisedUsers,
             "UncredentialedFilteringFallbackForSupervisedUsers",
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kWaitUntilAccessTokenAvailableForClassifyUrl,
             "WaitUntilAccessTokenAvailableForClassifyUrl",
#if BUILDFLAG(IS_ANDROID)
             // Android enforces at the OS level that supervised users must have
             // valid sign in credentials (and triggers a reauth if not). We can
             // therefore wait for a valid access token to be available before
             // calling ClassifyUrl, to avoid window conditions where the access
             // token is not yet available (eg. during startup).
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             // Other platforms don't enforce this, and we therefore cannot
             // wait for access tokens in Chrome.
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kReplaceSupervisionPrefsWithAccountCapabilitiesOnIOS,
             "ReplaceSupervisionPrefsWithAccountCapabilitiesOnIOS",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kReplaceSupervisionSystemCapabilitiesWithAccountCapabilitiesOnIOS,
             "ReplaceSupervisionSystemCapabilitiesWithAccountCapabilitiesOnIOS",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif
}  // namespace supervised_user
