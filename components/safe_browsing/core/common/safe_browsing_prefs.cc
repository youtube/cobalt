// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace {

// Update the correct UMA metric based on which pref was changed and which UI
// the change was made on.
void RecordExtendedReportingPrefChanged(
    const PrefService& prefs,
    safe_browsing::ExtendedReportingOptInLocation location) {
  bool pref_value = safe_browsing::IsExtendedReportingEnabled(prefs);

  switch (location) {
    case safe_browsing::SBER_OPTIN_SITE_CHROME_SETTINGS:
      UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.Extended.ChromeSettings",
                            pref_value);
      break;
    case safe_browsing::SBER_OPTIN_SITE_ANDROID_SETTINGS:
      UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.Extended.AndroidSettings",
                            pref_value);
      break;
    case safe_browsing::SBER_OPTIN_SITE_DOWNLOAD_FEEDBACK_POPUP:
      UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.Extended.DownloadPopup",
                            pref_value);
      break;
    case safe_browsing::SBER_OPTIN_SITE_SECURITY_INTERSTITIAL:
      UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.Extended.SecurityInterstitial",
                            pref_value);
      break;
    default:
      NOTREACHED();
  }
}

// A helper function to return a GURL containing just the scheme, host, port,
// and path from a URL. Equivalent to clearing any username, password, query,
// and ref. Return empty URL if |url| is not valid.
GURL GetSimplifiedURL(const GURL& url) {
  if (!url.is_valid() || !url.IsStandard())
    return GURL();

  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  replacements.ClearRef();

  return url.ReplaceComponents(replacements);
}

}  // namespace

namespace prefs {
const char kSafeBrowsingCsdPingTimestamps[] =
    "safebrowsing.csd_ping_timestamps";
const char kSafeBrowsingCsdPhishingProtectionAllowedByPolicy[] =
    "safebrowsing.csd_phishing_protection_allowed_by_policy";
const char kSafeBrowsingDeepScanningEnabled[] =
    "safebrowsing.deep_scanning_enabled";
const char kSafeBrowsingEnabled[] = "safebrowsing.enabled";
const char kSafeBrowsingEnhanced[] = "safebrowsing.enhanced";
const char kSafeBrowsingEnterpriseRealTimeUrlCheckMode[] =
    "safebrowsing.enterprise_real_time_url_check_mode";
const char kSafeBrowsingEnterpriseRealTimeUrlCheckScope[] =
    "safebrowsing.enterprise_real_time_url_check_scope";
const char kSafeBrowsingEsbProtegoPingWithTokenLastLogTime[] =
    "safebrowsing.esb_protego_ping_with_token_last_log_time";
const char kSafeBrowsingEsbProtegoPingWithoutTokenLastLogTime[] =
    "safebrowsing.esb_protego_ping_without_token_last_log_time";
const char kSafeBrowsingExtendedReportingOptInAllowed[] =
    "safebrowsing.extended_reporting_opt_in_allowed";
const char kSafeBrowsingIncidentsSent[] = "safebrowsing.incidents_sent";
const char kSafeBrowsingProceedAnywayDisabled[] =
    "safebrowsing.proceed_anyway_disabled";
const char kSafeBrowsingSawInterstitialScoutReporting[] =
    "safebrowsing.saw_interstitial_sber2";
const char kSafeBrowsingScoutReportingEnabled[] =
    "safebrowsing.scout_reporting_enabled";
const char kSafeBrowsingSurveysEnabled[] = "safebrowsing.surveys_enabled";
const char kSafeBrowsingTriggerEventTimestamps[] =
    "safebrowsing.trigger_event_timestamps";
const char kSafeBrowsingUnhandledGaiaPasswordReuses[] =
    "safebrowsing.unhandled_sync_password_reuses";
const char kSafeBrowsingNextPasswordCaptureEventLogTime[] =
    "safebrowsing.next_password_capture_event_log_time";
const char kSafeBrowsingAllowlistDomains[] =
    "safebrowsing.safe_browsing_whitelist_domains";
const char kPasswordProtectionChangePasswordURL[] =
    "safebrowsing.password_protection_change_password_url";
const char kPasswordProtectionLoginURLs[] =
    "safebrowsing.password_protection_login_urls";
const char kPasswordProtectionWarningTrigger[] =
    "safebrowsing.password_protection_warning_trigger";
const char kAdvancedProtectionLastRefreshInUs[] =
    "safebrowsing.advanced_protection_last_refresh";
const char kAdvancedProtectionAllowed[] =
    "safebrowsing.advanced_protection_allowed";
const char kSafeBrowsingMetricsLastLogTime[] =
    "safebrowsing.metrics_last_log_time";
const char kSafeBrowsingEventTimestamps[] = "safebrowsing.event_timestamps";
const char kSafeBrowsingHashRealTimeOhttpExpirationTime[] =
    "safebrowsing.hash_real_time_ohttp_expiration_time";
const char kSafeBrowsingHashRealTimeOhttpKey[] =
    "safebrowsing.hash_real_time_ohttp_key";
const char kAccountTailoredSecurityUpdateTimestamp[] =
    "safebrowsing.aesb_update_time_windows_epoch_micros";
const char kTailoredSecurityNextSyncFlowTimestamp[] =
    "safebrowsing.aesb_next_sync_flow_timestamp";
const char kAccountTailoredSecurityShownNotification[] =
    "safebrowsing.aesb_shown_notification";
const char kTailoredSecuritySyncFlowLastRunTime[] =
    "safebrowsing.aesb_sync_flow_start_timestamp";
const char kTailoredSecuritySyncFlowLastUserInteractionState[] =
    "safebrowsing.aesb_sync_flow_last_user_interaction_state";
const char kTailoredSecuritySyncFlowRetryState[] =
    "safebrowsing.aesb_sync_flow_retry_state";
const char kTailoredSecuritySyncFlowObservedOutcomeUnsetTimestamp[] =
    "safebrowsing.aesb_sync_flow_observed_outcome_unset_timestamp";
const char kEnhancedProtectionEnabledViaTailoredSecurity[] =
    "safebrowsing.esb_enabled_via_tailored_security";
const char kExtensionTelemetryLastUploadTime[] =
    "safebrowsing.extension_telemetry_last_upload_time";
const char kExtensionTelemetryConfig[] =
    "safebrowsing.extension_telemetry_configuration";
const char kExtensionTelemetryFileData[] =
    "safebrowsing.extension_telemetry_file_data";
const char kRealTimeDownloadProtectionRequestAllowedByPolicy[] =
    "safebrowsing.real_time_download_protection_request_allowed_by_policy";
const char kSafeBrowsingExtensionProtectionAllowedByPolicy[] =
    "safebrowsing.extension_protection_allowed_by_policy";
const char kHashPrefixRealTimeChecksAllowedByPolicy[] =
    "safebrowsing.hash_prefix_real_time_checks_allowed_by_policy";
}  // namespace prefs

namespace safe_browsing {

SafeBrowsingState GetSafeBrowsingState(const PrefService& prefs) {
  if (IsEnhancedProtectionEnabled(prefs)) {
    return SafeBrowsingState::ENHANCED_PROTECTION;
  } else if (prefs.GetBoolean(prefs::kSafeBrowsingEnabled)) {
    return SafeBrowsingState::STANDARD_PROTECTION;
  } else {
    return SafeBrowsingState::NO_SAFE_BROWSING;
  }
}

void SetSafeBrowsingState(PrefService* prefs,
                          SafeBrowsingState state,
                          bool is_esb_enabled_in_sync) {
  if (state == SafeBrowsingState::ENHANCED_PROTECTION) {
    SetEnhancedProtectionPref(prefs, true);
    SetStandardProtectionPref(prefs, true);
    prefs->SetBoolean(prefs::kEnhancedProtectionEnabledViaTailoredSecurity,
                      is_esb_enabled_in_sync);
  } else if (state == SafeBrowsingState::STANDARD_PROTECTION) {
    SetEnhancedProtectionPref(prefs, false);
    SetStandardProtectionPref(prefs, true);
  } else {
    SetEnhancedProtectionPref(prefs, false);
    SetStandardProtectionPref(prefs, false);
  }
}

bool IsSafeBrowsingEnabled(const PrefService& prefs) {
  return prefs.GetBoolean(prefs::kSafeBrowsingEnabled);
}

bool IsEnhancedProtectionEnabled(const PrefService& prefs) {
  // SafeBrowsingEnabled is checked too due to devices being out
  // of sync or not on a version that includes SafeBrowsingEnhanced pref.
  return prefs.GetBoolean(prefs::kSafeBrowsingEnhanced) &&
         IsSafeBrowsingEnabled(prefs);
}

bool ExtendedReportingPrefExists(const PrefService& prefs) {
  return prefs.HasPrefPath(prefs::kSafeBrowsingScoutReportingEnabled);
}

ExtendedReportingLevel GetExtendedReportingLevel(const PrefService& prefs) {
  return IsExtendedReportingEnabled(prefs) ? SBER_LEVEL_SCOUT : SBER_LEVEL_OFF;
}

bool IsExtendedReportingOptInAllowed(const PrefService& prefs) {
  return prefs.GetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed);
}

bool IsExtendedReportingEnabled(const PrefService& prefs) {
  return (IsSafeBrowsingEnabled(prefs) &&
          prefs.GetBoolean(prefs::kSafeBrowsingScoutReportingEnabled)) ||
         IsEnhancedProtectionEnabled(prefs);
}

bool IsExtendedReportingPolicyManaged(const PrefService& prefs) {
  return prefs.IsManagedPreference(prefs::kSafeBrowsingScoutReportingEnabled);
}

bool IsSafeBrowsingPolicyManaged(const PrefService& prefs) {
  return prefs.IsManagedPreference(prefs::kSafeBrowsingEnabled) ||
         prefs.IsManagedPreference(prefs::kSafeBrowsingEnhanced);
}

bool IsSafeBrowsingExtensionControlled(const PrefService& prefs) {
  // Checking only kSafeBrowsingEnabled since there is no extension API
  // that can control the kSafeBrowsingEnhanced protection pref.
  return prefs.FindPreference(prefs::kSafeBrowsingEnabled)
             ->IsExtensionControlled();
}

bool IsRealTimeDownloadProtectionRequestAllowed(const PrefService& prefs) {
  return prefs.GetBoolean(
      prefs::kRealTimeDownloadProtectionRequestAllowedByPolicy);
}

bool IsCsdPhishingProtectionAllowed(const PrefService& prefs) {
  return prefs.GetBoolean(
      prefs::kSafeBrowsingCsdPhishingProtectionAllowedByPolicy);
}

bool IsSafeBrowsingExtensionProtectionAllowed(const PrefService& prefs) {
  return prefs.GetBoolean(
      prefs::kSafeBrowsingExtensionProtectionAllowedByPolicy);
}

bool AreHashPrefixRealTimeLookupsAllowedByPolicy(const PrefService& prefs) {
  return prefs.GetBoolean(prefs::kHashPrefixRealTimeChecksAllowedByPolicy);
}

bool AreDeepScansAllowedByPolicy(const PrefService& prefs) {
  return prefs.GetBoolean(prefs::kSafeBrowsingDeepScanningEnabled);
}

bool IsSafeBrowsingSurveysEnabled(const PrefService& prefs) {
  return prefs.GetBoolean(prefs::kSafeBrowsingSurveysEnabled);
}

bool IsSafeBrowsingProceedAnywayDisabled(const PrefService& prefs) {
  return prefs.GetBoolean(prefs::kSafeBrowsingProceedAnywayDisabled);
}

void RecordExtendedReportingMetrics(const PrefService& prefs) {
  // This metric tracks the extended browsing opt-in based on whichever setting
  // the user is currently seeing. It tells us whether extended reporting is
  // happening for this user.
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.Extended",
                        IsExtendedReportingEnabled(prefs));
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kSafeBrowsingCsdPingTimestamps);
  registry->RegisterBooleanPref(
      prefs::kSafeBrowsingCsdPhishingProtectionAllowedByPolicy, true);
  registry->RegisterBooleanPref(prefs::kSafeBrowsingScoutReportingEnabled,
                                false);
  registry->RegisterBooleanPref(
      prefs::kSafeBrowsingSawInterstitialScoutReporting, false);
  registry->RegisterBooleanPref(
      prefs::kSafeBrowsingExtendedReportingOptInAllowed, true);
  registry->RegisterTimePref(
      prefs::kSafeBrowsingEsbProtegoPingWithTokenLastLogTime, base::Time());
  registry->RegisterTimePref(
      prefs::kSafeBrowsingEsbProtegoPingWithoutTokenLastLogTime, base::Time());
  registry->RegisterBooleanPref(
      prefs::kSafeBrowsingEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kSafeBrowsingEnhanced, false);
  registry->RegisterBooleanPref(prefs::kSafeBrowsingProceedAnywayDisabled,
                                false);
  registry->RegisterDictionaryPref(prefs::kSafeBrowsingIncidentsSent);
  registry->RegisterDictionaryPref(
      prefs::kSafeBrowsingUnhandledGaiaPasswordReuses);
  registry->RegisterStringPref(
      prefs::kSafeBrowsingNextPasswordCaptureEventLogTime,
      "0");  // int64 as string
  registry->RegisterListPref(prefs::kSafeBrowsingAllowlistDomains);
  registry->RegisterStringPref(prefs::kPasswordProtectionChangePasswordURL, "");
  registry->RegisterListPref(prefs::kPasswordProtectionLoginURLs);
  registry->RegisterIntegerPref(prefs::kPasswordProtectionWarningTrigger,
                                PASSWORD_PROTECTION_OFF);
  registry->RegisterInt64Pref(prefs::kAdvancedProtectionLastRefreshInUs, 0);
  registry->RegisterBooleanPref(prefs::kAdvancedProtectionAllowed, true);
  registry->RegisterIntegerPref(
      prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode,
      REAL_TIME_CHECK_DISABLED);
  registry->RegisterIntegerPref(
      prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckScope, 0);
  registry->RegisterInt64Pref(prefs::kSafeBrowsingMetricsLastLogTime, 0);
  registry->RegisterDictionaryPref(prefs::kSafeBrowsingEventTimestamps);
  registry->RegisterTimePref(
      prefs::kSafeBrowsingHashRealTimeOhttpExpirationTime, base::Time());
  registry->RegisterStringPref(prefs::kSafeBrowsingHashRealTimeOhttpKey, "");
  registry->RegisterTimePref(
      prefs::kAccountTailoredSecurityUpdateTimestamp, base::Time(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccountTailoredSecurityShownNotification, false);
  registry->RegisterBooleanPref(
      prefs::kEnhancedProtectionEnabledViaTailoredSecurity, false);
  registry->RegisterTimePref(prefs::kTailoredSecuritySyncFlowLastRunTime,
                             base::Time());
  registry->RegisterTimePref(prefs::kTailoredSecurityNextSyncFlowTimestamp,
                             base::Time());
  // TODO(crbug.com/1469133): remove sync flow last user interaction pref.
  registry->RegisterIntegerPref(
      prefs::kTailoredSecuritySyncFlowLastUserInteractionState,
      TailoredSecurityRetryState::UNSET);
  registry->RegisterIntegerPref(prefs::kTailoredSecuritySyncFlowRetryState,
                                TailoredSecurityRetryState::UNSET);
  registry->RegisterTimePref(
      prefs::kTailoredSecuritySyncFlowObservedOutcomeUnsetTimestamp,
      base::Time());

  registry->RegisterTimePref(prefs::kExtensionTelemetryLastUploadTime,
                             base::Time::Now());
  registry->RegisterDictionaryPref(prefs::kExtensionTelemetryConfig);
  registry->RegisterDictionaryPref(prefs::kExtensionTelemetryFileData);
  registry->RegisterBooleanPref(
      prefs::kRealTimeDownloadProtectionRequestAllowedByPolicy, true);
  registry->RegisterBooleanPref(
      prefs::kSafeBrowsingExtensionProtectionAllowedByPolicy, true);
  registry->RegisterBooleanPref(prefs::kHashPrefixRealTimeChecksAllowedByPolicy,
                                true);
  registry->RegisterBooleanPref(prefs::kSafeBrowsingSurveysEnabled, true);
  registry->RegisterBooleanPref(prefs::kSafeBrowsingDeepScanningEnabled, true);
}

const base::Value::Dict& GetExtensionTelemetryConfig(const PrefService& prefs) {
  return prefs.GetDict(prefs::kExtensionTelemetryConfig);
}

const base::Value::Dict& GetExtensionTelemetryFileData(
    const PrefService& prefs) {
  return prefs.GetDict(prefs::kExtensionTelemetryFileData);
}

void SetExtensionTelemetryConfig(PrefService& prefs,
                                 const base::Value::Dict& config) {
  prefs.SetDict(prefs::kExtensionTelemetryConfig, config.Clone());
}

base::Time GetLastUploadTimeForExtensionTelemetry(PrefService& prefs) {
  return (prefs.GetTime(prefs::kExtensionTelemetryLastUploadTime));
}

void SetLastUploadTimeForExtensionTelemetry(PrefService& prefs,
                                            const base::Time& time) {
  prefs.SetTime(prefs::kExtensionTelemetryLastUploadTime, time);
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kSafeBrowsingTriggerEventTimestamps);
}

void SetExtendedReportingPrefAndMetric(
    PrefService* prefs,
    bool value,
    ExtendedReportingOptInLocation location) {
  prefs->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled, value);
  RecordExtendedReportingPrefChanged(*prefs, location);
}

void SetExtendedReportingPrefForTests(PrefService* prefs, bool value) {
  prefs->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled, value);
}

void SetEnhancedProtectionPrefForTests(PrefService* prefs, bool value) {
  // SafeBrowsingEnabled pref needs to be turned on in order for enhanced
  // protection pref to be turned on. This method is only used for tests.
  prefs->SetBoolean(prefs::kSafeBrowsingEnabled, value);
  prefs->SetBoolean(prefs::kSafeBrowsingEnhanced, value);
}

void SetEnhancedProtectionPref(PrefService* prefs, bool value) {
  prefs->SetBoolean(prefs::kSafeBrowsingEnhanced, value);
}

void SetStandardProtectionPref(PrefService* prefs, bool value) {
  prefs->SetBoolean(prefs::kSafeBrowsingEnabled, value);
}

void UpdatePrefsBeforeSecurityInterstitial(PrefService* prefs) {
  // Remember that this user saw an interstitial.
  prefs->SetBoolean(prefs::kSafeBrowsingSawInterstitialScoutReporting, true);
}

base::Value::List GetSafeBrowsingPreferencesList(PrefService* prefs) {
  base::Value::List preferences_list;

  const char* safe_browsing_preferences[] = {
      prefs::kSafeBrowsingEnabled,
      prefs::kSafeBrowsingExtendedReportingOptInAllowed,
      prefs::kSafeBrowsingScoutReportingEnabled, prefs::kSafeBrowsingEnhanced};

  // Add the status of the preferences if they are Enabled or Disabled for the
  // user.
  for (const char* preference : safe_browsing_preferences) {
    preferences_list.Append(preference);
    bool enabled = prefs->GetBoolean(preference);
    preferences_list.Append(enabled ? "Enabled" : "Disabled");
  }
  return preferences_list;
}

base::Value::List GetSafeBrowsingPoliciesList(PrefService* prefs) {
  base::Value::List preferences_list;
  const base::Value::List& allowlist_domains =
      prefs->GetList(prefs::kSafeBrowsingAllowlistDomains);
  std::vector<std::string> domain_list;
  CanonicalizeDomainList(allowlist_domains, &domain_list);
  std::string domains;
  for (const auto& domain : domain_list) {
    domains = domains + " " + domain;
  }
  preferences_list.Append(domains);
  preferences_list.Append(prefs::kSafeBrowsingAllowlistDomains);
  preferences_list.Append(
      prefs->GetString(prefs::kPasswordProtectionChangePasswordURL));
  preferences_list.Append(prefs::kPasswordProtectionChangePasswordURL);
  preferences_list.Append(base::NumberToString(
      prefs->GetInteger(prefs::kPasswordProtectionWarningTrigger)));
  preferences_list.Append(prefs::kPasswordProtectionWarningTrigger);

  std::vector<GURL> login_urls_list;
  GetPasswordProtectionLoginURLsPref(*prefs, &login_urls_list);
  std::string login_urls;
  for (const auto& login_url : login_urls_list) {
    login_urls = login_urls + " " + login_url.spec();
  }
  preferences_list.Append(login_urls);
  preferences_list.Append(prefs::kPasswordProtectionLoginURLs);

  preferences_list.Append(prefs->GetBoolean(
      prefs::kRealTimeDownloadProtectionRequestAllowedByPolicy));
  preferences_list.Append(
      prefs::kRealTimeDownloadProtectionRequestAllowedByPolicy);
  preferences_list.Append(prefs->GetBoolean(
      prefs::kSafeBrowsingCsdPhishingProtectionAllowedByPolicy));
  preferences_list.Append(
      prefs::kSafeBrowsingCsdPhishingProtectionAllowedByPolicy);
  preferences_list.Append(prefs->GetBoolean(
      prefs::kSafeBrowsingExtensionProtectionAllowedByPolicy));
  preferences_list.Append(
      prefs::kSafeBrowsingExtensionProtectionAllowedByPolicy);
  preferences_list.Append(
      prefs->GetBoolean(prefs::kHashPrefixRealTimeChecksAllowedByPolicy));
  preferences_list.Append(prefs::kHashPrefixRealTimeChecksAllowedByPolicy);
  preferences_list.Append(
      prefs->GetBoolean(prefs::kSafeBrowsingSurveysEnabled));
  preferences_list.Append(prefs::kSafeBrowsingSurveysEnabled);
  return preferences_list;
}

void GetSafeBrowsingAllowlistDomainsPref(
    const PrefService& prefs,
    std::vector<std::string>* out_canonicalized_domain_list) {
  const base::Value::List& pref_value =
      prefs.GetList(prefs::kSafeBrowsingAllowlistDomains);
  CanonicalizeDomainList(pref_value, out_canonicalized_domain_list);
}

void CanonicalizeDomainList(
    const base::Value::List& raw_domain_list,
    std::vector<std::string>* out_canonicalized_domain_list) {
  out_canonicalized_domain_list->clear();
  for (const base::Value& value : raw_domain_list) {
    // Verify if it is valid domain string.
    url::CanonHostInfo host_info;
    std::string canonical_host =
        net::CanonicalizeHost(value.GetString(), &host_info);
    if (!canonical_host.empty())
      out_canonicalized_domain_list->push_back(canonical_host);
  }
}

bool IsURLAllowlistedByPolicy(const GURL& url, const PrefService& pref) {
  if (!pref.HasPrefPath(prefs::kSafeBrowsingAllowlistDomains))
    return false;
  const base::Value::List& allowlist =
      pref.GetList(prefs::kSafeBrowsingAllowlistDomains);
  for (const base::Value& value : allowlist) {
    if (url.DomainIs(value.GetString()))
      return true;
  }
  return false;
}

std::vector<std::string> GetURLAllowlistByPolicy(PrefService* pref_service) {
  std::vector<std::string> allowlist_domains;
  const base::Value::List& allowlist =
      pref_service->GetList(prefs::kSafeBrowsingAllowlistDomains);
  for (const base::Value& value : allowlist) {
    allowlist_domains.push_back(value.GetString());
  }
  return allowlist_domains;
}

bool MatchesEnterpriseAllowlist(const PrefService& pref,
                                const std::vector<GURL>& url_chain) {
  for (const GURL& url : url_chain) {
    if (IsURLAllowlistedByPolicy(url, pref))
      return true;
  }
  return false;
}

void GetPasswordProtectionLoginURLsPref(const PrefService& prefs,
                                        std::vector<GURL>* out_login_url_list) {
  const base::Value::List& pref_value =
      prefs.GetList(prefs::kPasswordProtectionLoginURLs);
  out_login_url_list->clear();
  for (const base::Value& value : pref_value) {
    GURL login_url(value.GetString());
    // Skip invalid or none-http/https login URLs.
    if (login_url.is_valid() && login_url.SchemeIsHTTPOrHTTPS())
      out_login_url_list->push_back(login_url);
  }
}

bool MatchesPasswordProtectionLoginURL(const GURL& url,
                                       const PrefService& prefs) {
  if (!url.is_valid())
    return false;

  std::vector<GURL> login_urls;
  GetPasswordProtectionLoginURLsPref(prefs, &login_urls);
  return MatchesURLList(url, login_urls);
}

bool MatchesURLList(const GURL& target_url, const std::vector<GURL> url_list) {
  if (url_list.empty() || !target_url.is_valid())
    return false;
  GURL simple_target_url = GetSimplifiedURL(target_url);
  for (const GURL& url : url_list) {
    if (GetSimplifiedURL(url) == simple_target_url) {
      return true;
    }
  }
  return false;
}

GURL GetPasswordProtectionChangePasswordURLPref(const PrefService& prefs) {
  if (!prefs.HasPrefPath(prefs::kPasswordProtectionChangePasswordURL))
    return GURL();
  GURL change_password_url_from_pref(
      prefs.GetString(prefs::kPasswordProtectionChangePasswordURL));
  // Skip invalid or non-http/https URL.
  if (change_password_url_from_pref.is_valid() &&
      change_password_url_from_pref.SchemeIsHTTPOrHTTPS()) {
    return change_password_url_from_pref;
  }

  return GURL();
}

bool MatchesPasswordProtectionChangePasswordURL(const GURL& url,
                                                const PrefService& prefs) {
  if (!url.is_valid())
    return false;

  GURL change_password_url = GetPasswordProtectionChangePasswordURLPref(prefs);
  if (change_password_url.is_empty())
    return false;

  return GetSimplifiedURL(change_password_url) == GetSimplifiedURL(url);
}

}  // namespace safe_browsing
