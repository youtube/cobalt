// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"

#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/reporting/reporting_service_settings.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#else
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#endif

namespace {

const char16_t kMaskedUsername[] = u"*****";

safe_browsing::EventResult GetEventResultFromThreatType(
    std::string threat_type) {
  if (threat_type == "ENTERPRISE_WARNED_SEEN") {
    return safe_browsing::EventResult::WARNED;
  }
  if (threat_type == "ENTERPRISE_WARNED_BYPASS") {
    return safe_browsing::EventResult::BYPASSED;
  }
  if (threat_type == "ENTERPRISE_BLOCKED_SEEN") {
    return safe_browsing::EventResult::BLOCKED;
  }
  NOTREACHED();
  return safe_browsing::EventResult::UNKNOWN;
}

void AddAnalysisConnectorVerdictToEvent(
    const enterprise_connectors::ContentAnalysisResponse::Result& result,
    base::Value::Dict& event) {
  base::Value::List triggered_rule_info;
  for (const enterprise_connectors::TriggeredRule& trigger :
       result.triggered_rules()) {
    base::Value::Dict triggered_rule;
    triggered_rule.Set(
        extensions::SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName,
        trigger.rule_name());
    triggered_rule.Set(
        extensions::SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleId,
        trigger.rule_id());
    triggered_rule.Set(
        extensions::SafeBrowsingPrivateEventRouter::kKeyUrlCategory,
        trigger.url_category());

    triggered_rule_info.Append(std::move(triggered_rule));
  }
  event.Set(extensions::SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo,
            std::move(triggered_rule_info));
}

std::string ActionFromVerdictType(
    safe_browsing::RTLookupResponse::ThreatInfo::VerdictType verdict_type) {
  if (verdict_type == safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS) {
    return "BLOCK";
  }
  if (verdict_type == safe_browsing::RTLookupResponse::ThreatInfo::WARN) {
    return "WARN";
  }
  return "ACTION_UNKNOWN";
}

void AddTriggeredRuleInfoToUrlFilteringInterstitialEvent(
    const safe_browsing::RTLookupResponse& response,
    base::Value::Dict& event) {
  base::Value::List triggered_rule_info;

  for (const safe_browsing::RTLookupResponse::ThreatInfo& threat_info :
       response.threat_info()) {
    base::Value::Dict triggered_rule;
    triggered_rule.Set(
        extensions::SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName,
        threat_info.matched_url_navigation_rule().rule_name());
    triggered_rule.Set(
        extensions::SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleId,
        threat_info.matched_url_navigation_rule().rule_id());
    triggered_rule.Set(
        extensions::SafeBrowsingPrivateEventRouter::kKeyUrlCategory,
        threat_info.matched_url_navigation_rule().matched_url_category());
    triggered_rule.Set(extensions::SafeBrowsingPrivateEventRouter::kKeyAction,
                       ActionFromVerdictType(threat_info.verdict_type()));

    triggered_rule_info.Append(std::move(triggered_rule));
  }
  event.Set(extensions::SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo,
            std::move(triggered_rule_info));
}

std::string MalwareRuleToThreatType(const std::string& rule_name) {
  if (rule_name == "uws") {
    return "POTENTIALLY_UNWANTED";
  } else if (rule_name == "malware") {
    return "DANGEROUS";
  } else {
    return "UNKNOWN";
  }
}

std::string DangerTypeToThreatType(download::DownloadDangerType danger_type) {
  switch (danger_type) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return "DANGEROUS_FILE_TYPE";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      return "DANGEROUS_URL";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return "DANGEROUS";
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return "UNCOMMON";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return "DANGEROUS_HOST";
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return "POTENTIALLY_UNWANTED";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
      return "DANGEROUS_ACCOUNT_COMPROMISE";
    default:
      // This can be reached when reporting an opened download that doesn't have
      // a verdict yet.
      return "UNKNOWN";
  }
}

// Do a best-effort masking of `username`. If it's an email address (such as
// foo@example.com), everything before @ should be masked. Otherwise, the entire
// username should be masked.
std::u16string MaskUsername(const std::u16string& username) {
  size_t pos = username.find(u"@");
  if (pos == std::string::npos) {
    return std::u16string(kMaskedUsername);
  }

  return std::u16string(kMaskedUsername) + username.substr(pos);
}

// Create a URLMatcher representing the filters in
// `settings.enabled_opt_in_events` for `event_type`. This field of the
// reporting settings connector contains a map where keys are event types and
// values are lists of URL patterns specifying on which URLs the events are
// allowed to be reported. An event is generated iff its event type is present
// in the opt-in events field and the URL it relates to matches at least one of
// the event type's filters.
std::unique_ptr<url_matcher::URLMatcher> CreateURLMatcherForOptInEvent(
    const enterprise_connectors::ReportingSettings& settings,
    const char* event_type) {
  const auto& it = settings.enabled_opt_in_events.find(event_type);
  if (it == settings.enabled_opt_in_events.end()) {
    return nullptr;
  }

  std::unique_ptr<url_matcher::URLMatcher> matcher =
      std::make_unique<url_matcher::URLMatcher>();
  base::MatcherStringPattern::ID unused_id(0);
  url_matcher::util::AddFilters(matcher.get(), true, &unused_id, it->second);

  return matcher;
}

bool IsOptInEventEnabled(url_matcher::URLMatcher* matcher, const GURL& url) {
  return matcher && !matcher->MatchURL(url).empty();
}

}  // namespace

namespace extensions {

// Key names used with when building the dictionary to pass to the real-time
// reporting API.
const char SafeBrowsingPrivateEventRouter::kKeyUrl[] = "url";
const char SafeBrowsingPrivateEventRouter::kKeySource[] = "source";
const char SafeBrowsingPrivateEventRouter::kKeyDestination[] = "destination";
const char SafeBrowsingPrivateEventRouter::kKeyUserName[] = "userName";
const char SafeBrowsingPrivateEventRouter::kKeyIsPhishingUrl[] =
    "isPhishingUrl";
const char SafeBrowsingPrivateEventRouter::kKeyProfileUserName[] =
    "profileUserName";
const char SafeBrowsingPrivateEventRouter::kKeyFileName[] = "fileName";
const char SafeBrowsingPrivateEventRouter::kKeyDownloadDigestSha256[] =
    "downloadDigestSha256";
const char SafeBrowsingPrivateEventRouter::kKeyReason[] = "reason";
const char SafeBrowsingPrivateEventRouter::kKeyNetErrorCode[] = "netErrorCode";
const char SafeBrowsingPrivateEventRouter::kKeyClickedThrough[] =
    "clickedThrough";
const char SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName[] = "ruleName";
const char SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleId[] = "ruleId";
const char SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo[] =
    "triggeredRuleInfo";
const char SafeBrowsingPrivateEventRouter::kKeyThreatType[] = "threatType";
const char SafeBrowsingPrivateEventRouter::kKeyContentType[] = "contentType";
const char SafeBrowsingPrivateEventRouter::kKeyContentSize[] = "contentSize";
const char SafeBrowsingPrivateEventRouter::kKeyTrigger[] = "trigger";
const char SafeBrowsingPrivateEventRouter::kKeyEventResult[] = "eventResult";
const char SafeBrowsingPrivateEventRouter::kKeyMalwareFamily[] =
    "malwareFamily";
const char SafeBrowsingPrivateEventRouter::kKeyMalwareCategory[] =
    "malwareCategory";
const char SafeBrowsingPrivateEventRouter::kKeyEvidenceLockerFilePath[] =
    "evidenceLockerFilepath";
const char SafeBrowsingPrivateEventRouter::kKeyScanId[] = "scanId";
const char SafeBrowsingPrivateEventRouter::kKeyIsFederated[] = "isFederated";
const char SafeBrowsingPrivateEventRouter::kKeyFederatedOrigin[] =
    "federatedOrigin";
const char SafeBrowsingPrivateEventRouter::kKeyLoginUserName[] =
    "loginUserName";
const char SafeBrowsingPrivateEventRouter::kKeyPasswordBreachIdentities[] =
    "identities";
const char SafeBrowsingPrivateEventRouter::kKeyPasswordBreachIdentitiesUrl[] =
    "url";
const char
    SafeBrowsingPrivateEventRouter::kKeyPasswordBreachIdentitiesUsername[] =
        "username";
const char SafeBrowsingPrivateEventRouter::kKeyUserJustification[] =
    "userJustification";
const char SafeBrowsingPrivateEventRouter::kKeyUrlCategory[] = "urlCategory";
const char SafeBrowsingPrivateEventRouter::kKeyAction[] = "action";

// All new event names should be added to the array
// `enterprise_connectors::ReportingServiceSettings::kAllReportingEvents` in
// `chrome/browser/enterprise/connectors/reporting/reporting_service_settings.h`
const char SafeBrowsingPrivateEventRouter::kKeyUrlFilteringInterstitialEvent[] =
    "urlFilteringInterstitialEvent";
const char SafeBrowsingPrivateEventRouter::kKeyPasswordReuseEvent[] =
    "passwordReuseEvent";
const char SafeBrowsingPrivateEventRouter::kKeyPasswordChangedEvent[] =
    "passwordChangedEvent";
const char SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent[] =
    "dangerousDownloadEvent";
const char SafeBrowsingPrivateEventRouter::kKeyInterstitialEvent[] =
    "interstitialEvent";
const char SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent[] =
    "sensitiveDataEvent";
const char SafeBrowsingPrivateEventRouter::kKeyUnscannedFileEvent[] =
    "unscannedFileEvent";
const char SafeBrowsingPrivateEventRouter::kKeyLoginEvent[] = "loginEvent";
const char SafeBrowsingPrivateEventRouter::kKeyPasswordBreachEvent[] =
    "passwordBreachEvent";

const char SafeBrowsingPrivateEventRouter::kKeyUnscannedReason[] =
    "unscannedReason";

const char SafeBrowsingPrivateEventRouter::kTriggerFileDownload[] =
    "FILE_DOWNLOAD";
const char SafeBrowsingPrivateEventRouter::kTriggerFileUpload[] = "FILE_UPLOAD";
const char SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload[] =
    "WEB_CONTENT_UPLOAD";
const char SafeBrowsingPrivateEventRouter::kTriggerPagePrint[] = "PAGE_PRINT";
const char SafeBrowsingPrivateEventRouter::kTriggerFileTransfer[] =
    "FILE_TRANSFER";

SafeBrowsingPrivateEventRouter::SafeBrowsingPrivateEventRouter(
    content::BrowserContext* context)
    : context_(context) {
  event_router_ = EventRouter::Get(context_);
  identity_manager_ = IdentityManagerFactory::GetForProfile(
      Profile::FromBrowserContext(context_));
  reporting_client_ =
      enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
          context);
}

SafeBrowsingPrivateEventRouter::~SafeBrowsingPrivateEventRouter() = default;

// static
std::string SafeBrowsingPrivateEventRouter::GetBaseName(
    const std::string& filename) {
  base::FilePath::StringType os_filename;
#if BUILDFLAG(IS_WIN)
  os_filename = base::UTF8ToWide(filename);
#else
  os_filename = filename;
#endif
  return base::FilePath(os_filename).BaseName().AsUTF8Unsafe();
}

void SafeBrowsingPrivateEventRouter::OnPolicySpecifiedPasswordReuseDetected(
    const GURL& url,
    const std::string& user_name,
    bool is_phishing_url,
    bool warning_shown) {
  api::safe_browsing_private::PolicySpecifiedPasswordReuse params;
  params.url = url.spec();
  params.user_name = user_name;
  params.is_phishing_url = is_phishing_url;

  // |event_router_| can be null in tests.
  if (event_router_) {
    base::Value::List event_value;
    event_value.Append(params.ToValue());

    auto extension_event = std::make_unique<Event>(
        events::
            SAFE_BROWSING_PRIVATE_ON_POLICY_SPECIFIED_PASSWORD_REUSE_DETECTED,
        api::safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected::
            kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  absl::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyPasswordReuseEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, params.url);
  event.Set(kKeyUserName, params.user_name);
  event.Set(kKeyIsPhishingUrl, params.is_phishing_url);
  event.Set(kKeyProfileUserName, GetProfileUserName());
  event.Set(kKeyEventResult,
            safe_browsing::EventResultToString(
                warning_shown ? safe_browsing::EventResult::WARNED
                              : safe_browsing::EventResult::ALLOWED));

  reporting_client_->ReportRealtimeEvent(
      kKeyPasswordReuseEvent, std::move(settings.value()), std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnPolicySpecifiedPasswordChanged(
    const std::string& user_name) {
  // |event_router_| can be null in tests.
  if (event_router_) {
    base::Value::List event_value;
    event_value.Append(user_name);
    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_POLICY_SPECIFIED_PASSWORD_CHANGED,
        api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::
            kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  absl::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyPasswordChangedEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUserName, user_name);
  event.Set(kKeyProfileUserName, GetProfileUserName());

  reporting_client_->ReportRealtimeEvent(
      kKeyPasswordChangedEvent, std::move(settings.value()), std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadOpened(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& scan_id,
    const download::DownloadDangerType danger_type,
    const int64_t content_size) {
  api::safe_browsing_private::DangerousDownloadInfo params;
  params.url = url.spec();
  params.file_name = file_name;
  params.download_digest_sha256 = download_digest_sha256;
  params.user_name = GetProfileUserName();

  // |event_router_| can be null in tests.
  if (event_router_) {
    base::Value::List event_value;
    event_value.Append(params.ToValue());

    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_DANGEROUS_DOWNLOAD_OPENED,
        api::safe_browsing_private::OnDangerousDownloadOpened::kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  absl::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyDangerousDownloadEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, params.url);
  event.Set(kKeyFileName, GetBaseName(params.file_name));
  event.Set(kKeyDownloadDigestSha256, params.download_digest_sha256);
  event.Set(kKeyProfileUserName, params.user_name);
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, kTriggerFileDownload);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(
                                 safe_browsing::EventResult::BYPASSED));
  event.Set(kKeyClickedThrough, true);
  event.Set(kKeyThreatType, DangerTypeToThreatType(danger_type));
  // The scan ID can be empty when the reported dangerous download is from a
  // Safe Browsing verdict.
  if (!scan_id.empty()) {
    event.Set(kKeyScanId, scan_id);
  }

  reporting_client_->ReportRealtimeEvent(kKeyDangerousDownloadEvent,
                                         std::move(settings.value()),
                                         std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnSecurityInterstitialShown(
    const GURL& url,
    const std::string& reason,
    int net_error_code) {
  api::safe_browsing_private::InterstitialInfo params;
  params.url = url.spec();
  params.reason = reason;
  if (net_error_code < 0) {
    params.net_error_code = base::NumberToString(net_error_code);
  }
  params.user_name = GetProfileUserName();

  // |event_router_| can be null in tests.
  if (event_router_) {
    base::Value::List event_value;
    event_value.Append(params.ToValue());

    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_SECURITY_INTERSTITIAL_SHOWN,
        api::safe_browsing_private::OnSecurityInterstitialShown::kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  absl::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyInterstitialEvent) == 0) {
    return;
  }

  PrefService* prefs = Profile::FromBrowserContext(context_)->GetPrefs();
  safe_browsing::EventResult event_result =
      prefs->GetBoolean(prefs::kSafeBrowsingProceedAnywayDisabled)
          ? safe_browsing::EventResult::BLOCKED
          : safe_browsing::EventResult::WARNED;
  base::Value::Dict event;
  event.Set(kKeyUrl, params.url);
  event.Set(kKeyReason, params.reason);
  event.Set(kKeyNetErrorCode, net_error_code);
  event.Set(kKeyProfileUserName, params.user_name);
  event.Set(kKeyClickedThrough, false);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(event_result));

  reporting_client_->ReportRealtimeEvent(
      kKeyInterstitialEvent, std::move(settings.value()), std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnSecurityInterstitialProceeded(
    const GURL& url,
    const std::string& reason,
    int net_error_code) {
  api::safe_browsing_private::InterstitialInfo params;
  params.url = url.spec();
  params.reason = reason;
  if (net_error_code < 0) {
    params.net_error_code = base::NumberToString(net_error_code);
  }
  params.user_name = GetProfileUserName();

  // |event_router_| can be null in tests.
  if (event_router_) {
    base::Value::List event_value;
    event_value.Append(params.ToValue());

    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_SECURITY_INTERSTITIAL_PROCEEDED,
        api::safe_browsing_private::OnSecurityInterstitialProceeded::kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  absl::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyInterstitialEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, params.url);
  event.Set(kKeyReason, params.reason);
  event.Set(kKeyNetErrorCode, net_error_code);
  event.Set(kKeyProfileUserName, params.user_name);
  event.Set(kKeyClickedThrough, true);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(
                                 safe_browsing::EventResult::BYPASSED));

  reporting_client_->ReportRealtimeEvent(
      kKeyInterstitialEvent, std::move(settings.value()), std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnAnalysisConnectorResult(
    const GURL& url,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& scan_id,
    safe_browsing::DeepScanAccessPoint /* access_point */,
    const enterprise_connectors::ContentAnalysisResponse::Result& result,
    const int64_t content_size,
    safe_browsing::EventResult event_result) {
  if (result.tag() == "malware") {
    DCHECK_EQ(1, result.triggered_rules().size());
    OnDangerousDeepScanningResult(
        url, source, destination, file_name, download_digest_sha256,
        MalwareRuleToThreatType(result.triggered_rules(0).rule_name()),
        mime_type, trigger, content_size, event_result, result.malware_family(),
        result.malware_category(), result.evidence_locker_filepath(), scan_id);
  } else if (result.tag() == "dlp") {
    OnSensitiveDataEvent(url, source, destination, file_name,
                         download_digest_sha256, mime_type, trigger, scan_id,
                         result, content_size, event_result);
  }
}

void SafeBrowsingPrivateEventRouter::OnDangerousDeepScanningResult(
    const GURL& url,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& threat_type,
    const std::string& mime_type,
    const std::string& trigger,
    const int64_t content_size,
    safe_browsing::EventResult event_result,
    const std::string& malware_family,
    const std::string& malware_category,
    const std::string& evidence_locker_filepath,
    const std::string& scan_id) {
  absl::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyDangerousDownloadEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeySource, source);
  event.Set(kKeyDestination, destination);
  event.Set(kKeyFileName, GetBaseName(file_name));
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyProfileUserName, GetProfileUserName());
  event.Set(kKeyThreatType, threat_type);
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, trigger);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(event_result));
  event.Set(kKeyClickedThrough,
            event_result == safe_browsing::EventResult::BYPASSED);
  if (!malware_family.empty()) {
    event.Set(kKeyMalwareFamily, malware_family);
  }
  if (!malware_category.empty()) {
    event.Set(kKeyMalwareCategory, malware_category);
  }
  if (!evidence_locker_filepath.empty()) {
    event.Set(kKeyEvidenceLockerFilePath, evidence_locker_filepath);
  }
  // The scan ID can be empty when the reported dangerous download is from a
  // Safe Browsing verdict.
  if (!scan_id.empty()) {
    event.Set(kKeyScanId, scan_id);
  }

  reporting_client_->ReportRealtimeEvent(kKeyDangerousDownloadEvent,
                                         std::move(settings.value()),
                                         std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnSensitiveDataEvent(
    const GURL& url,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& scan_id,
    const enterprise_connectors::ContentAnalysisResponse::Result& result,
    const int64_t content_size,
    safe_browsing::EventResult event_result) {
  absl::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeySensitiveDataEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeySource, source);
  event.Set(kKeyDestination, destination);
  event.Set(kKeyFileName, GetBaseName(file_name));
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyProfileUserName, GetProfileUserName());
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, trigger);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(event_result));
  event.Set(kKeyClickedThrough,
            event_result == safe_browsing::EventResult::BYPASSED);
  if (!result.evidence_locker_filepath().empty()) {
    event.Set(kKeyEvidenceLockerFilePath, result.evidence_locker_filepath());
  }
  event.Set(kKeyScanId, scan_id);

  AddAnalysisConnectorVerdictToEvent(result, event);

  reporting_client_->ReportRealtimeEvent(
      kKeySensitiveDataEvent, std::move(settings.value()), std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnAnalysisConnectorWarningBypassed(
    const GURL& url,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& scan_id,
    safe_browsing::DeepScanAccessPoint access_point,
    const enterprise_connectors::ContentAnalysisResponse::Result& result,
    const int64_t content_size,
    absl::optional<std::u16string> user_justification) {
  absl::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeySensitiveDataEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeySource, source);
  event.Set(kKeyDestination, destination);
  event.Set(kKeyFileName, GetBaseName(file_name));
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyProfileUserName, GetProfileUserName());
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, trigger);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(
                                 safe_browsing::EventResult::BYPASSED));
  event.Set(kKeyClickedThrough, true);
  if (!result.evidence_locker_filepath().empty()) {
    event.Set(kKeyEvidenceLockerFilePath, result.evidence_locker_filepath());
  }
  event.Set(kKeyScanId, scan_id);
  if (user_justification) {
    event.Set(kKeyUserJustification, *user_justification);
  }

  AddAnalysisConnectorVerdictToEvent(result, event);

  reporting_client_->ReportRealtimeEvent(
      kKeySensitiveDataEvent, std::move(settings.value()), std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnUnscannedFileEvent(
    const GURL& url,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    safe_browsing::DeepScanAccessPoint access_point,
    const std::string& reason,
    const int64_t content_size,
    safe_browsing::EventResult event_result) {
  absl::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyUnscannedFileEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeySource, source);
  event.Set(kKeyDestination, destination);
  event.Set(kKeyFileName, GetBaseName(file_name));
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyProfileUserName, GetProfileUserName());
  event.Set(kKeyContentType, mime_type);
  event.Set(kKeyUnscannedReason, reason);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, trigger);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(event_result));
  event.Set(kKeyClickedThrough,
            event_result == safe_browsing::EventResult::BYPASSED);

  reporting_client_->ReportRealtimeEvent(
      kKeyUnscannedFileEvent, std::move(settings.value()), std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadEvent(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const download::DownloadDangerType danger_type,
    const std::string& mime_type,
    const std::string& scan_id,
    const int64_t content_size,
    safe_browsing::EventResult event_result) {
  OnDangerousDownloadEvent(url, file_name, download_digest_sha256,
                           DangerTypeToThreatType(danger_type), mime_type,
                           scan_id, content_size, event_result);
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadEvent(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& threat_type,
    const std::string& mime_type,
    const std::string& scan_id,
    const int64_t content_size,
    safe_browsing::EventResult event_result) {
  absl::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyDangerousDownloadEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyFileName, GetBaseName(file_name));
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyProfileUserName, GetProfileUserName());
  event.Set(kKeyThreatType, threat_type);
  event.Set(kKeyClickedThrough, false);
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, kTriggerFileDownload);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(event_result));

  // The scan ID can be empty when the reported dangerous download is from a
  // Safe Browsing verdict.
  if (!scan_id.empty()) {
    event.Set(kKeyScanId, scan_id);
  }

  reporting_client_->ReportRealtimeEvent(kKeyDangerousDownloadEvent,
                                         std::move(settings.value()),
                                         std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadWarningBypassed(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const download::DownloadDangerType danger_type,
    const std::string& mime_type,
    const std::string& scan_id,
    const int64_t content_size) {
  OnDangerousDownloadWarningBypassed(url, file_name, download_digest_sha256,
                                     DangerTypeToThreatType(danger_type),
                                     mime_type, scan_id, content_size);
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadWarningBypassed(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& threat_type,
    const std::string& mime_type,
    const std::string& scan_id,
    const int64_t content_size) {
  absl::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyDangerousDownloadEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyFileName, GetBaseName(file_name));
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyProfileUserName, GetProfileUserName());
  event.Set(kKeyThreatType, threat_type);
  event.Set(kKeyClickedThrough, true);
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, kTriggerFileDownload);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(
                                 safe_browsing::EventResult::BYPASSED));
  // The scan ID can be empty when the reported dangerous download is from a
  // Safe Browsing verdict.
  if (!scan_id.empty()) {
    event.Set(kKeyScanId, scan_id);
  }

  reporting_client_->ReportRealtimeEvent(kKeyDangerousDownloadEvent,
                                         std::move(settings.value()),
                                         std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnLoginEvent(
    const GURL& url,
    bool is_federated,
    const url::Origin& federated_origin,
    const std::u16string& username) {
  absl::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value()) {
    return;
  }

  std::unique_ptr<url_matcher::URLMatcher> matcher =
      CreateURLMatcherForOptInEvent(settings.value(), kKeyLoginEvent);
  if (!IsOptInEventEnabled(matcher.get(), url)) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyIsFederated, is_federated);
  if (is_federated) {
    event.Set(kKeyFederatedOrigin, federated_origin.Serialize());
  }
  event.Set(kKeyProfileUserName, GetProfileUserName());
  event.Set(kKeyLoginUserName, MaskUsername(username));

  reporting_client_->ReportRealtimeEvent(
      kKeyLoginEvent, std::move(settings.value()), std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnPasswordBreach(
    const std::string& trigger,
    const std::vector<std::pair<GURL, std::u16string>>& identities) {
  absl::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value()) {
    return;
  }

  std::unique_ptr<url_matcher::URLMatcher> matcher =
      CreateURLMatcherForOptInEvent(settings.value(), kKeyPasswordBreachEvent);
  if (!matcher) {
    return;
  }

  base::Value::Dict event;
  base::Value::List identities_list;
  event.Set(kKeyTrigger, trigger);
  for (const std::pair<GURL, std::u16string>& i : identities) {
    if (!IsOptInEventEnabled(matcher.get(), i.first)) {
      continue;
    }

    base::Value::Dict identity;
    identity.Set(kKeyPasswordBreachIdentitiesUrl, i.first.spec());
    identity.Set(kKeyPasswordBreachIdentitiesUsername, MaskUsername(i.second));
    identities_list.Append(std::move(identity));
  }

  if (identities_list.empty()) {
    // Don't send an empty event if none of the breached identities matched a
    // pattern in the URL filters.
    return;
  }

  event.Set(kKeyPasswordBreachIdentities, std::move(identities_list));
  event.Set(kKeyProfileUserName, GetProfileUserName());

  reporting_client_->ReportRealtimeEvent(
      kKeyPasswordBreachEvent, std::move(settings.value()), std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnUrlFilteringInterstitial(
    const GURL& url,
    const std::string& threat_type,
    const safe_browsing::RTLookupResponse& response) {
  absl::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() || settings->enabled_event_names.count(
                                   kKeyUrlFilteringInterstitialEvent) == 0) {
    return;
  }
  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyProfileUserName, GetProfileUserName());
  safe_browsing::EventResult event_result =
      GetEventResultFromThreatType(threat_type);
  event.Set(kKeyClickedThrough,
            event_result == safe_browsing::EventResult::BYPASSED);
  event.Set(kKeyThreatType, threat_type);
  AddTriggeredRuleInfoToUrlFilteringInterstitialEvent(response, event);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(event_result));

  reporting_client_->ReportRealtimeEvent(kKeyUrlFilteringInterstitialEvent,
                                         std::move(settings.value()),
                                         std::move(event));
}

void SafeBrowsingPrivateEventRouter::SetIdentityManagerForTesting(
    signin::IdentityManager* identity_manager) {
  identity_manager_ = identity_manager;
}

std::string SafeBrowsingPrivateEventRouter::GetProfileUserName() const {
  return safe_browsing::GetProfileEmail(identity_manager_);
}

}  // namespace extensions
