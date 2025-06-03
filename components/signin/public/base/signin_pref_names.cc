// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_pref_names.h"

#include "build/chromeos_buildflags.h"

namespace prefs {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// A boolean pref - should unauthenticated user should be logged out
// automatically. Default value is false.
const char kForceLogoutUnauthenticatedUserEnabled[] =
    "profile.force_logout_unauthenticated_user_enabled";

// An integer property indicating the state of account id migration from
// email to gaia id for the the profile.  See account_tracker_service.h
// for possible values.
const char kAccountIdMigrationState[] = "account_id_migration_state";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Name of the preference property that persists the account information
// tracked by this signin.
const char kAccountInfo[] = "account_info";

// Boolean identifying whether reverse auto-login is enabled.
const char kAutologinEnabled[] = "autologin.enabled";

// A hash of the GAIA accounts present in the content area. Order does not
// affect the hash, but signed in/out status will. Stored as the Base64 string.
const char kGaiaCookieHash[] = "gaia_cookie.hash";

// The last time that kGaiaCookieHash was last updated. Stored as a double that
// should be converted into base::Time.
const char kGaiaCookieChangedTime[] = "gaia_cookie.changed_time";

// The last time that periodic reporting occured, to allow us to report as close
// to once per intended interval as possible, through restarts. Stored as a
// double that should be converted into base::Time.
const char kGaiaCookiePeriodicReportTime[] = "gaia_cookie.periodic_report_time";

// Typically contains an obfuscated gaiaid. Some platforms may have
// an email stored in this preference instead. This is transitional and will
// eventually be fixed.
const char kGoogleServicesAccountId[] = "google.services.account_id";

// Boolean indicating if the user gave consent for Sync.
const char kGoogleServicesConsentedToSync[] =
    "google.services.consented_to_sync";

// Similar to kGoogleServicesLastSyncingUsername, this is the corresponding
// version of kGoogleServicesAccountId that is not cleared on signout.
// DEPRECATED: this preference is deprecated and is always empty. It will be
// removed once all users are migrated to `kGoogleServicesLastSyncingGaiaId`.
const char kGoogleServicesLastSyncingAccountIdDeprecated[] =
    "google.services.last_account_id";

// Similar to `kGoogleServicesLastSyncingUsername` that is not cleared on
// signout. Note this is always a Gaia ID, as opposed to
// `kGoogleServicesAccountId` which may be an email.
const char kGoogleServicesLastSyncingGaiaId[] = "google.services.last_gaia_id";

// String the identifies the last user that logged into sync and other
// google services. This value is not cleared on signout.
// This pref remains in order to pre-fill the sign in page when reconnecting a
// profile, but programmatic checks to see if a given account is the same as the
// last account should use `kGoogleServicesLastSyncingGaiaId` instead.
const char kGoogleServicesLastSyncingUsername[] =
    "google.services.last_username";

// Device id scoped to single signin. This device id will be regenerated if user
// signs out and signs back in. When refresh token is requested for this user it
// will be annotated with this device id.
const char kGoogleServicesSigninScopedDeviceId[] =
    "google.services.signin_scoped_device_id";

// Local state pref containing a string regex that restricts which accounts
// can be used to log in to chrome (e.g. "*@google.com"). If missing or blank,
// all accounts are allowed (no restrictions).
const char kGoogleServicesUsernamePattern[] =
    "google.services.username_pattern";

// List to keep track of emails for which the user has rejected one-click
// sign-in.
const char kReverseAutologinRejectedEmailList[] =
    "reverse_autologin.rejected_email_list";

// Boolean indicating if this profile was signed in with information from a
// credential provider.
const char kSignedInWithCredentialProvider[] =
    "signin.with_credential_provider";

// Boolean which stores if the user is allowed to signin to chrome.
const char kSigninAllowed[] = "signin.allowed";

// Contains last |ListAccounts| data which corresponds to Gaia cookies.
const char kGaiaCookieLastListAccountsData[] =
    "gaia_cookie.last_list_accounts_data";

// List of patterns to determine the account visibility.
const char kRestrictAccountsToPatterns[] =
    "signin.restrict_accounts_to_patterns";

// Boolean which indicates if the user is allowed to sign into Chrome on the
// next startup.
const char kSigninAllowedOnNextStartup[] = "signin.allowed_on_next_startup";

// String that represent the url for which cookies will have to be moved to a
// newly created profile via signin interception.
const char kSigninInterceptionIDPCookiesUrl[] =
    "signin.interception.idp_cookies.url";

// Integer which indicates whether enterprise profile separation is enforced or
// disabled.
const char kProfileSeparationSettings[] = "profile_separation.settings";

// Integer which indicates which options users have for their existing data when
// creating a new profile via the enterprise profile separation flow.
const char kProfileSeparationDataMigrationSettings[] =
    "profile_separation.data_migration_settings";

// List of domains that are not required to create a new profile after a content
// area signin.
const char kProfileSeparationDomainExceptionList[] =
    "profile_separation.domain_exception_list";

// Response set by chrome://policy/test for
// UserCloudSigninRestrictionPolicyFetcher::GetManagedAccountsSigninRestriction.
// This is only used on Canary and for testing.
const char kUserCloudSigninPolicyResponseFromPolicyTestPage[] =
    "signin.user_cloud_signin_policy_response_from_policy_test_page";

}  // namespace prefs
