// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_switches.h"

#include "base/feature_list.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"

namespace switches {

// All switches in alphabetical order.

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kCctSignInPrompt,
             "CctSignInPrompt",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Migrate usages of USM flag to force child account sign-in to use the account
// capability `IsSubjectToParentalControls`.
BASE_FEATURE(kForceSupervisedSigninWithCapabilities,
             "ForceSupervisedSigninWithCapabilities",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Add some history opt-in entry points on Android.
BASE_FEATURE(kHistoryOptInEntryPoints,
             "HistoryOptInEntryPoints",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Add history opt-in IPH in settings on Android.
BASE_FEATURE(kHistoryOptInIph,
             "HistoryOptInIph",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature to bypass double-checking that signin callers have correctly gotten
// the user to accept account management. This check is slow and not strictly
// necessary, so disable it while we work on adding caching.
// TODO(https://crbug.com/339457762): Restore the check when we implement
// caching.
BASE_FEATURE(kSkipCheckForAccountManagementOnSignin,
             "SkipCheckForAccountManagementOnSignin",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUnoForAuto, "UnoForAuto", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseHostedDomainForManagementCheckOnSignin,
             "UseHostedDomainForManagementCheckOnSignin",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Force enable the default browser step in the first run experience on Desktop.
const char kForceFreDefaultBrowserStep[] = "force-fre-default-browser-step";
#endif

// Clears the token service before using it. This allows simulating the
// expiration of credentials during testing.
const char kClearTokenService[] = "clear-token-service";

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
// Enable experimental binding session credentials to the device.
BASE_FEATURE(kEnableBoundSessionCredentials,
             "EnableBoundSessionCredentials",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsBoundSessionCredentialsEnabled(const PrefService* profile_prefs) {
  // Enterprise policy takes precedence over the feature value.
  if (profile_prefs->HasPrefPath(prefs::kBoundSessionCredentialsEnabled)) {
    return profile_prefs->GetBoolean(prefs::kBoundSessionCredentialsEnabled);
  }

  return base::FeatureList::IsEnabled(kEnableBoundSessionCredentials);
}

const base::FeatureParam<EnableBoundSessionCredentialsDiceSupport>::Option
    enable_bound_session_credentials_dice_support[] = {
        {EnableBoundSessionCredentialsDiceSupport::kDisabled, "disabled"},
        {EnableBoundSessionCredentialsDiceSupport::kEnabled, "enabled"}};
const base::FeatureParam<EnableBoundSessionCredentialsDiceSupport>
    kEnableBoundSessionCredentialsDiceSupport{
        &kEnableBoundSessionCredentials, "dice-support",
        EnableBoundSessionCredentialsDiceSupport::kEnabled,
        &enable_bound_session_credentials_dice_support};

// Restricts the DBSC registration URL path to a single allowed string.
// Set to "/" to denote an empty path.
// Set to an empty string to remove the restriction.
const base::FeatureParam<std::string>
    kEnableBoundSessionCredentialsExclusiveRegistrationPath{
        &kEnableBoundSessionCredentials, "exclusive-registration-path",
        "/RegisterSession"};

// Enables Chrome refresh tokens binding to a device.
BASE_FEATURE(kEnableChromeRefreshTokenBinding,
             "EnableChromeRefreshTokenBinding",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsChromeRefreshTokenBindingEnabled(const PrefService* profile_prefs) {
  // Enterprise policy takes precedence over the feature value.
  if (profile_prefs->HasPrefPath(prefs::kBoundSessionCredentialsEnabled)) {
    return profile_prefs->GetBoolean(prefs::kBoundSessionCredentialsEnabled);
  }

  return base::FeatureList::IsEnabled(kEnableChromeRefreshTokenBinding);
}
#endif

BASE_FEATURE(kEnablePreferencesAccountStorage,
             "EnablePreferencesAccountStorage",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// This feature disables all extended sync promos.
BASE_FEATURE(kForceDisableExtendedSyncPromos,
             "ForceDisableExtendedSyncPromos",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// Features to trigger the startup sign-in promo at boot.
BASE_FEATURE(kForceStartupSigninPromo,
             "ForceStartupSigninPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kExplicitBrowserSigninUIOnDesktop,
             "ExplicitBrowserSigninUIOnDesktop",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

const base::FeatureParam<bool> kInterceptBubblesDismissibleByAvatarButton{
    &kExplicitBrowserSigninUIOnDesktop,
    /*name=*/"bubble_dismissible_by_avatar_button",
    /*default_value=*/true};

bool IsExplicitBrowserSigninUIOnDesktopEnabled() {
  return base::FeatureList::IsEnabled(kExplicitBrowserSigninUIOnDesktop);
}

BASE_FEATURE(kImprovedSigninUIOnDesktop,
             "ImprovedSigninUIOnDesktop",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsImprovedSigninUIOnDesktopEnabled() {
  return IsExplicitBrowserSigninUIOnDesktopEnabled() &&
         base::FeatureList::IsEnabled(kImprovedSigninUIOnDesktop);
}

BASE_FEATURE(kImprovedSettingsUIOnDesktop,
             "ImprovedSettingsUIOnDesktop",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsImprovedSettingsUIOnDesktopEnabled() {
  return IsExplicitBrowserSigninUIOnDesktopEnabled() &&
         base::FeatureList::IsEnabled(kImprovedSettingsUIOnDesktop);
}

BASE_FEATURE(kEnableSnackbarInSettings,
             "EnableSnackbarInSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableImprovedGuestProfileMenu,
             "EnableImprovedGuestProfileMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)

BASE_FEATURE(kEnableIdentityInAuthError,
             "EnableIdentityInAuthError",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableErrorBadgeOnIdentityDisc,
             "EnableErrorBadgeOnIdentityDisc",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableASWebAuthenticationSession,
             "EnableASWebAuthenticationSession",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
BASE_FEATURE(kBatchUploadDesktop,
             "BatchUploadDesktop",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

bool IsBatchUploadDesktopEnabled() {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  return base::FeatureList::IsEnabled(kBatchUploadDesktop);
#else
  return false;
#endif
}

// Enables showing the enterprise dialog after every signin into a managed
// account.
BASE_FEATURE(kShowEnterpriseDialogForAllManagedAccountsSignin,
             "ShowEnterpriseDialogForAllManagedAccountsSignin",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace switches

#if BUILDFLAG(IS_CHROMEOS)
// Enables the generation of pseudo-stable per-user per-device device
// identifiers. This identifier can be reset by the user by powerwashing the
// device.
BASE_FEATURE(kStableDeviceId,
             "StableDeviceId",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_MIRROR) && !BUILDFLAG(IS_IOS)
BASE_FEATURE(kVerifyRequestInitiatorForMirrorHeaders,
             "VerifyRequestInitiatorForMirrorHeaders",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_MIRROR) && !BUILDFLAG(IS_IOS)

BASE_FEATURE(kProfilesReordering,
             "ProfilesReordering",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOutlineSilhouetteIcon,
             "OutlineSilhouetteIcon",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kIgnoreMirrorHeadersInBackgoundTabs,
             "IgnoreMirrorHeadersInBackgoundTabs",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kNonDefaultGaiaOriginCheck,
             "NonDefaultGaiaOriginCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);
