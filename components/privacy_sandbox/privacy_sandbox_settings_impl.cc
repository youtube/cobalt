// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_settings_impl.h"
#include <cstddef>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/browsing_topics/common/common_types.h"
#include "components/browsing_topics/common/semantic_tree.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/tpcd_experiment_eligibility.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/site_for_cookies.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace privacy_sandbox {

namespace {

constexpr char kBlockedTopicsTopicKey[] = "topic";
constexpr char kBlockedTopicsBlockTimeKey[] = "blockedOn";

constexpr char kIsTopicsAllowedHistogram[] = "PrivacySandbox.IsTopicsAllowed";
constexpr char kIsTopicsAllowedForContextHistogram[] =
    "PrivacySandbox.IsTopicsAllowedForContext";
constexpr char kIsAttributionReportingEverAllowedHistogram[] =
    "PrivacySandbox.IsAttributionReportingEverAllowed";
constexpr char kIsAttributionReportingAllowedHistogram[] =
    "PrivacySandbox.IsAttributionReportingAllowed";
constexpr char kMaySendAttributionReportHistogram[] =
    "PrivacySandbox.MaySendAttributionReport";
constexpr char kIsFledgeJoinAllowedHistogram[] =
    "PrivacySandbox.IsFledgeJoinAllowed";
constexpr char kIsFledgeLeaveAllowedHistogram[] =
    "PrivacySandbox.IsFledgeLeaveAllowed";
constexpr char kIsFledgeUpdateAllowedHistogram[] =
    "PrivacySandbox.IsFledgeUpdateAllowed";
constexpr char kIsFledgeSellAllowedHistogram[] =
    "PrivacySandbox.IsFledgeSellAllowed";
constexpr char kIsFledgeBuyAllowedHistogram[] =
    "PrivacySandbox.IsFledgeBuyAllowed";
constexpr char kIsPrivacySandboxReportingDestinationAttestedHistogram[] =
    "PrivacySandbox.IsPrivacySandboxReportingDestinationAttested";
constexpr char kIsSharedStorageAllowedHistogram[] =
    "PrivacySandbox.IsSharedStorageAllowed";
constexpr char kIsSharedStorageSelectURLAllowedHistogram[] =
    "PrivacySandbox.IsSharedStorageSelectURLAllowed";
constexpr char kIsPrivateAggregationAllowedHistogram[] =
    "PrivacySandbox.IsPrivateAggregationAllowed";

bool IsCookiesClearOnExitEnabled(HostContentSettingsMap* map) {
  return map->GetDefaultContentSetting(ContentSettingsType::COOKIES) ==
         ContentSetting::CONTENT_SETTING_SESSION_ONLY;
}

// Convert a stored FLEDGE block eTLD+1 into applicable content settings
// patterns. This ensures that if Public Suffix List membership changes, the
// stored item continues to match as when it was set. Multiple patterns are set
// to support IP address fallbacks, which do not support [*.] prefixes.
// TODO (crbug.com/1287153): This is somewhat hacky and can be removed when
// FLEDGE is controlled by a content setting directly.
std::vector<ContentSettingsPattern> FledgeBlockToContentSettingsPatterns(
    const std::string& entry) {
  return {ContentSettingsPattern::FromString("[*.]" + entry),
          ContentSettingsPattern::FromString(entry)};
}

// Returns a base::Value::Dict for storage in prefs that represents |topic|
// blocked at the current time.
base::Value::Dict CreateBlockedTopicEntry(const CanonicalTopic& topic) {
  return base::Value::Dict()
      .Set(kBlockedTopicsTopicKey, topic.ToValue())
      .Set(kBlockedTopicsBlockTimeKey, base::TimeToValue(base::Time::Now()));
}

std::set<browsing_topics::Topic> GetTopicsSetFromString(
    std::string topics_string) {
  if (topics_string.empty()) {
    return {};
  }
  std::set<browsing_topics::Topic> result;
  std::vector<std::string> tokens = base::SplitString(
      topics_string, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const std::string& token : tokens) {
    int topic_id;
    CHECK(base::StringToInt(token, &topic_id));
    result.emplace(topic_id);
  }
  return result;
}

}  // namespace

// static
bool PrivacySandboxSettingsImpl::IsAllowed(Status status) {
  return status == Status::kAllowed;
}

// static
void PrivacySandboxSettingsImpl::JoinHistogram(const char* name,
                                               Status status) {
  base::UmaHistogramEnumeration(name, status);
}

// static
void PrivacySandboxSettingsImpl::JoinFledgeHistogram(
    content::InterestGroupApiOperation interest_group_api_operation,
    Status status) {
  switch (interest_group_api_operation) {
    case content::InterestGroupApiOperation::kJoin:
      JoinHistogram(kIsFledgeJoinAllowedHistogram, status);
      break;
    case content::InterestGroupApiOperation::kLeave:
      JoinHistogram(kIsFledgeLeaveAllowedHistogram, status);
      break;
    case content::InterestGroupApiOperation::kUpdate:
      JoinHistogram(kIsFledgeUpdateAllowedHistogram, status);
      break;
    case content::InterestGroupApiOperation::kSell:
      JoinHistogram(kIsFledgeSellAllowedHistogram, status);
      break;
    case content::InterestGroupApiOperation::kBuy:
      JoinHistogram(kIsFledgeBuyAllowedHistogram, status);
      break;
  }
}

PrivacySandboxSettingsImpl::PrivacySandboxSettingsImpl(
    std::unique_ptr<Delegate> delegate,
    HostContentSettingsMap* host_content_settings_map,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    TrackingProtectionSettings* tracking_protection_settings,
    PrefService* pref_service)
    : delegate_(std::move(delegate)),
      host_content_settings_map_(host_content_settings_map),
      cookie_settings_(cookie_settings),
      tracking_protection_settings_(tracking_protection_settings),
      pref_service_(pref_service) {
  CHECK(pref_service_);
  CHECK(host_content_settings_map_);
  CHECK(cookie_settings_);
  CHECK(tracking_protection_settings_);
  // "Clear on exit" causes a cookie deletion on shutdown. But for practical
  // purposes, we're notifying the observers on startup (which should be
  // equivalent, as no cookie operations could have happened while the profile
  // was shut down).
  if (IsCookiesClearOnExitEnabled(host_content_settings_map_)) {
    OnCookiesCleared();
  }

  tracking_protection_settings_observation_.Observe(
      tracking_protection_settings_);

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
      base::BindRepeating(
          &PrivacySandboxSettingsImpl::OnRelatedWebsiteSetsEnabledPrefChanged,
          base::Unretained(this)));
}

PrivacySandboxSettingsImpl::~PrivacySandboxSettingsImpl() = default;

PrivacySandboxSettingsImpl::Status
PrivacySandboxSettingsImpl::GetM1TopicAllowedStatus() const {
  auto control_status = GetM1PrivacySandboxApiEnabledStatus(
      prefs::kPrivacySandboxM1TopicsEnabled);
  auto has_appropriate_consent = delegate_->HasAppropriateTopicsConsent();

  // If `control_status` indicates that Topics is allowed, then
  // `has_appropriate_consent` should be true, as there is no pathway for a user
  // to enable Topics controls without also granting consent. The inverse does
  // not hold, as an extension or policy may disable Topics, without necessarily
  // revoking user consent.
  if (control_status == Status::kAllowed && !has_appropriate_consent) {
    // This status will be recorded via UMA, and is indicative of an error.
    return Status::kMismatchedConsent;
  }

  return control_status;
}

const std::set<browsing_topics::Topic>&
PrivacySandboxSettingsImpl::GetFinchDisabledTopics() {
  if (finch_disabled_topics_.size() > 0) {
    return finch_disabled_topics_;
  }
  std::string disabled_topics_string =
      blink::features::kBrowsingTopicsDisabledTopicsList.Get();
  finch_disabled_topics_ = GetTopicsSetFromString(disabled_topics_string);
  return finch_disabled_topics_;
}

const std::set<browsing_topics::Topic>&
PrivacySandboxSettingsImpl::GetFinchPrioritizedTopics() {
  if (finch_prioritized_topics_.size() > 0) {
    return finch_prioritized_topics_;
  }
  std::string prioritized_topics_string =
      blink::features::kBrowsingTopicsPrioritizedTopicsList.Get();
  finch_prioritized_topics_ = GetTopicsSetFromString(prioritized_topics_string);
  return finch_prioritized_topics_;
}

bool PrivacySandboxSettingsImpl::IsTopicsAllowed() const {
  // M1 specific
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4)) {
    Status status = GetM1TopicAllowedStatus();
    JoinHistogram(kIsTopicsAllowedHistogram, status);
    return IsAllowed(status);
  }

  // Topics API calculation should be prevented if the user has blocked 3PC
  // cookies, as there will be no context specific check.
  const auto cookie_controls_mode =
      static_cast<content_settings::CookieControlsMode>(
          pref_service_->GetInteger(prefs::kCookieControlsMode));
  const auto default_content_setting =
      cookie_settings_->GetDefaultCookieSetting();

  const bool third_party_cookies_blocked =
      default_content_setting == ContentSetting::CONTENT_SETTING_BLOCK ||
      cookie_controls_mode ==
          content_settings::CookieControlsMode::kBlockThirdParty;
  return IsPrivacySandboxEnabled() && !third_party_cookies_blocked;
}

bool PrivacySandboxSettingsImpl::IsTopicsAllowedForContext(
    const url::Origin& top_frame_origin,
    const GURL& url,
    content::RenderFrameHost* console_frame) const {
  // Check for attestation on the calling context's site.
  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          net::SchemefulSite(url), PrivacySandboxAttestationsGatedAPI::kTopics);
  if (!IsAllowed(attestation_status)) {
    JoinHistogram(kIsTopicsAllowedForContextHistogram, attestation_status);
    if (console_frame) {
      console_frame->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          "Attestation check for Topics on " + url.spec() + " failed.");
    }
    return false;
  }

  // M1 specific
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4)) {
    Status status = GetM1TopicAllowedStatus();
    if (IsAllowed(status)) {
      status = GetSiteAccessAllowedStatus(top_frame_origin, url);
    }
    JoinHistogram(kIsTopicsAllowedForContextHistogram, status);
    return IsAllowed(status);
  }

  // If the Topics API is disabled completely, it is not available in any
  // context.
  return IsTopicsAllowed() &&
         IsPrivacySandboxEnabledForContext(top_frame_origin, url);
}

bool PrivacySandboxSettingsImpl::IsTopicAllowed(const CanonicalTopic& topic) {
  const auto& blocked_topics =
      pref_service_->GetList(prefs::kPrivacySandboxBlockedTopics);

  std::vector<browsing_topics::Topic> ancestor_topics =
      browsing_topics::SemanticTree().GetAncestorTopics(topic.topic_id());
  for (const base::Value& item : blocked_topics) {
    auto blocked_topic =
        CanonicalTopic::FromValue(*item.GetDict().Find(kBlockedTopicsTopicKey));
    if (!blocked_topic) {
      continue;
    }

    if ((topic.topic_id() == blocked_topic->topic_id()) ||
        (base::Contains(ancestor_topics, blocked_topic->topic_id()))) {
      return false;
    }
  }

  for (browsing_topics::Topic blocked_topic_id : GetFinchDisabledTopics()) {
    if ((topic.topic_id() == blocked_topic_id) ||
        (base::Contains(ancestor_topics, blocked_topic_id))) {
      return false;
    }
  }
  return true;
}

void PrivacySandboxSettingsImpl::SetTopicAllowed(const CanonicalTopic& topic,
                                                 bool allowed) {
  ScopedListPrefUpdate scoped_pref_update(pref_service_,
                                          prefs::kPrivacySandboxBlockedTopics);

  // Presence in the preference list indicates that a topic is blocked, as
  // there is no concept of explicitly allowed topics. Thus, allowing a topic
  // is the same as removing it, if it exists, from the blocklist. Blocking
  // a topic is the same as adding it to the blocklist, but as duplicate entries
  // are undesireable, removing any existing reference first is desireable.
  // Thus, regardless of |allowed|, removing any existing reference is the
  // first step.
  scoped_pref_update->EraseIf([&](const base::Value& value) {
    auto* blocked_topic_value = value.GetDict().Find(kBlockedTopicsTopicKey);
    auto converted_topic = CanonicalTopic::FromValue(*blocked_topic_value);
    return converted_topic && *converted_topic == topic;
  });

  // If the topic is being blocked, it can be (re)added to the blocklist. If the
  // topic was removed from the blocklist above, this is equivalent to updating
  // the modified time associated with the entry to the current time. As data
  // deletions are typically from the current time backwards, this makes it
  // more likely to be removed - a privacy improvement.
  if (!allowed) {
    scoped_pref_update->Append(CreateBlockedTopicEntry(topic));
  }
}

bool PrivacySandboxSettingsImpl::IsTopicPrioritized(
    const CanonicalTopic& topic) {
  const std::set<browsing_topics::Topic>& prioritized_topics =
      GetFinchPrioritizedTopics();
  if (prioritized_topics.contains(topic.topic_id())) {
    return true;
  }
  for (const browsing_topics::Topic& ancestor_topic :
       browsing_topics::SemanticTree().GetAncestorTopics(topic.topic_id())) {
    if (prioritized_topics.contains(ancestor_topic)) {
      return true;
    }
  }
  return false;
}

void PrivacySandboxSettingsImpl::ClearTopicSettings(base::Time start_time,
                                                    base::Time end_time) {
  ScopedListPrefUpdate scoped_pref_update(pref_service_,
                                          prefs::kPrivacySandboxBlockedTopics);

  // Shortcut for maximum time range deletion.
  if (start_time == base::Time() && end_time == base::Time::Max()) {
    scoped_pref_update->clear();
    return;
  }

  scoped_pref_update->EraseIf([&](const base::Value& value) {
    auto blocked_time =
        base::ValueToTime(value.GetDict().Find(kBlockedTopicsBlockTimeKey));
    return start_time <= blocked_time && blocked_time <= end_time;
  });
}

base::Time PrivacySandboxSettingsImpl::TopicsDataAccessibleSince() const {
  return pref_service_->GetTime(
      prefs::kPrivacySandboxTopicsDataAccessibleSince);
}

PrivacySandboxSettingsImpl::Status
PrivacySandboxSettingsImpl::GetM1AttributionReportingAllowedStatus(
    const url::Origin& top_frame_origin,
    const url::Origin& reporting_origin) const {
  Status status = GetM1PrivacySandboxApiEnabledStatus(
      prefs::kPrivacySandboxM1AdMeasurementEnabled);
  if (!IsAllowed(status)) {
    return status;
  }

  return GetSiteAccessAllowedStatus(top_frame_origin,
                                    reporting_origin.GetURL());
}

bool PrivacySandboxSettingsImpl::IsAttributionReportingEverAllowed() const {
  // M1 specific
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4)) {
    Status status = GetM1PrivacySandboxApiEnabledStatus(
        prefs::kPrivacySandboxM1AdMeasurementEnabled);
    JoinHistogram(kIsAttributionReportingEverAllowedHistogram, status);
    return IsAllowed(status);
  }

  return IsPrivacySandboxEnabled();
}

bool PrivacySandboxSettingsImpl::IsAttributionReportingAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& reporting_origin,
    content::RenderFrameHost* console_frame) const {
  // Check for attestation on the reporting origin.
  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          net::SchemefulSite(reporting_origin),
          PrivacySandboxAttestationsGatedAPI::kAttributionReporting);
  if (!IsAllowed(attestation_status)) {
    JoinHistogram(kIsAttributionReportingAllowedHistogram, attestation_status);
    if (console_frame) {
      console_frame->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          "Attestation check for Attribution Reporting on " +
              reporting_origin.Serialize() + " failed.");
    }
    return false;
  }

  // M1 specific
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4)) {
    Status status = GetM1AttributionReportingAllowedStatus(top_frame_origin,
                                                           reporting_origin);
    JoinHistogram(kIsAttributionReportingAllowedHistogram, status);
    return IsAllowed(status);
  }

  return IsPrivacySandboxEnabledForContext(top_frame_origin,
                                           reporting_origin.GetURL());
}

bool PrivacySandboxSettingsImpl::MaySendAttributionReport(
    const url::Origin& source_origin,
    const url::Origin& destination_origin,
    const url::Origin& reporting_origin,
    content::RenderFrameHost* console_frame) const {
  // Check for attestation on the reporting origin.
  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          net::SchemefulSite(reporting_origin),
          PrivacySandboxAttestationsGatedAPI::kAttributionReporting);
  if (!IsAllowed(attestation_status)) {
    JoinHistogram(kMaySendAttributionReportHistogram, attestation_status);
    if (console_frame) {
      console_frame->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          "Attestation check for Attribution Reporting on " +
              reporting_origin.Serialize() + " failed.");
    }
    return false;
  }

  // M1 specific
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4)) {
    Status status = GetM1AttributionReportingAllowedStatus(
        /*top_frame_origin=*/source_origin,
        /*reporting_origin=*/reporting_origin);
    if (IsAllowed(status)) {
      status = GetM1AttributionReportingAllowedStatus(
          /*top_frame_origin=*/destination_origin,
          /*reporting_origin=*/reporting_origin);
    }
    JoinHistogram(kMaySendAttributionReportHistogram, status);
    return IsAllowed(status);
  }

  // The |reporting_origin| needs to have been accessible in both source
  // and trigger contexts. These are both checked when they occur, but
  // user settings may have changed between then and when the attribution report
  // is sent.
  return IsPrivacySandboxEnabledForContext(/*top_frame_origin=*/source_origin,
                                           reporting_origin.GetURL()) &&
         IsPrivacySandboxEnabledForContext(
             /*top_frame_origin=*/destination_origin,
             reporting_origin.GetURL());
}

bool PrivacySandboxSettingsImpl::
    IsAttributionReportingTransitionalDebuggingAllowed(
        const url::Origin& top_frame_origin,
        const url::Origin& reporting_origin,
        bool& can_bypass) const {
  content_settings::CookieSettingsBase::CookieSettingWithMetadata
      cookie_setting_with_metadata;
  // Third party cookies must also be available for this context. An empty site
  // for cookies is provided so the context is always treated as a third party.
  bool allowed = cookie_settings_->IsFullCookieAccessAllowed(
      reporting_origin.GetURL(), net::SiteForCookies(), top_frame_origin,
      net::CookieSettingOverrides(), &cookie_setting_with_metadata);

  if (base::FeatureList::IsEnabled(
          kAttributionDebugReportingCookieDeprecationTesting)) {
    can_bypass =
        cookie_setting_with_metadata.BlockedByThirdPartyCookieBlocking() &&
        delegate_->AreThirdPartyCookiesBlockedByCookieDeprecationExperiment();
  } else {
    can_bypass = false;
  }
  return allowed;
}

void PrivacySandboxSettingsImpl::SetFledgeJoiningAllowed(
    const std::string& top_frame_etld_plus1,
    bool allowed) {
  ScopedDictPrefUpdate scoped_pref_update(
      pref_service_, prefs::kPrivacySandboxFledgeJoinBlocked);

  // Ensure that the provided etld_plus1 actually is an etld+1.
  auto effective_top_frame_etld_plus1 =
      net::registry_controlled_domains::GetDomainAndRegistry(
          top_frame_etld_plus1,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  // Hosts are also accepted as a fallback. This may occur if the private
  // registry has changed, and what the caller may be assuming is an eTLD+1 no
  // longer is. Simply ignoring non-eTLD+1's may thus result in unexpected
  // access.
  if (effective_top_frame_etld_plus1 != top_frame_etld_plus1) {
    // Add a dummy scheme and use GURL to confirm the provided string is a valid
    // host.
    const GURL url("https://" + top_frame_etld_plus1);
    effective_top_frame_etld_plus1 = url.host();
  }

  // Ignore attempts to configure an empty etld+1. This will also catch the
  // case where the eTLD+1 was not even a host, as GURL will have canonicalised
  // it to empty.
  if (effective_top_frame_etld_plus1.length() == 0) {
    NOTREACHED() << "Cannot control FLEDGE joining for empty eTLD+1";
    return;
  }

  if (allowed) {
    // Existence of the key implies blocking, so simply removing the key is
    // sufficient. If the key wasn't already present, the following is a no-op.
    scoped_pref_update->Remove(effective_top_frame_etld_plus1);
  } else {
    // Overriding the creation date for keys which already exist is acceptable.
    // Time range based deletions are typically started from the current time,
    // and so this will be more aggressively removed. This decreases the chance
    // a potentially sensitive website remains in preferences.
    scoped_pref_update->Set(effective_top_frame_etld_plus1,
                            base::TimeToValue(base::Time::Now()));
  }
}

void PrivacySandboxSettingsImpl::ClearFledgeJoiningAllowedSettings(
    base::Time start_time,
    base::Time end_time) {
  ScopedDictPrefUpdate scoped_pref_update(
      pref_service_, prefs::kPrivacySandboxFledgeJoinBlocked);
  auto& pref_data = scoped_pref_update.Get();

  // Shortcut for maximum time range deletion
  if (start_time == base::Time() && end_time == base::Time::Max()) {
    pref_data.clear();
    return;
  }

  std::vector<std::string> keys_to_remove;
  for (auto entry : pref_data) {
    absl::optional<base::Time> created_time = base::ValueToTime(entry.second);
    if (created_time.has_value() && start_time <= created_time &&
        created_time <= end_time) {
      keys_to_remove.push_back(entry.first);
    }
  }

  for (const auto& key : keys_to_remove) {
    pref_data.Remove(key);
  }
}

bool PrivacySandboxSettingsImpl::IsFledgeJoiningAllowed(
    const url::Origin& top_frame_origin) const {
  ScopedDictPrefUpdate scoped_pref_update(
      pref_service_, prefs::kPrivacySandboxFledgeJoinBlocked);
  auto& pref_data = scoped_pref_update.Get();
  for (auto entry : pref_data) {
    if (base::ranges::any_of(FledgeBlockToContentSettingsPatterns(entry.first),
                             [&](const auto& pattern) {
                               return pattern.Matches(
                                   top_frame_origin.GetURL());
                             })) {
      return false;
    }
  }
  return true;
}

PrivacySandboxSettingsImpl::Status
PrivacySandboxSettingsImpl::GetM1FledgeAllowedStatus(
    const url::Origin& top_frame_origin,
    const url::Origin& auction_party) const {
  Status status = GetM1PrivacySandboxApiEnabledStatus(
      prefs::kPrivacySandboxM1FledgeEnabled);
  if (!IsAllowed(status)) {
    return status;
  }

  return GetSiteAccessAllowedStatus(top_frame_origin, auction_party.GetURL());
}

bool PrivacySandboxSettingsImpl::IsEventReportingDestinationAttested(
    const url::Origin& destination_origin,
    privacy_sandbox::PrivacySandboxAttestationsGatedAPI invoking_api) const {
  // Check for attestation on the event recipient's site with whichever API
  // created the frame that invoked the event reporting.
  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          net::SchemefulSite(destination_origin), invoking_api);
  JoinHistogram(kIsPrivacySandboxReportingDestinationAttestedHistogram,
                attestation_status);
  return IsAllowed(attestation_status);
}

bool PrivacySandboxSettingsImpl::IsFledgeAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& auction_party,
    content::InterestGroupApiOperation interest_group_api_operation,
    content::RenderFrameHost* console_frame) const {
  // Check for attestation on the auction party's site. The auction party is a
  // variety of entities during the auction, all of which need to be attested.
  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          net::SchemefulSite(auction_party),
          PrivacySandboxAttestationsGatedAPI::kProtectedAudience);
  if (!IsAllowed(attestation_status)) {
    JoinFledgeHistogram(interest_group_api_operation, attestation_status);
    if (console_frame) {
      console_frame->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          "Attestation check for Protected Audience on " +
              auction_party.Serialize() + " failed.");
    }
    return false;
  }

  if (interest_group_api_operation ==
          content::InterestGroupApiOperation::kJoin &&
      !IsFledgeJoiningAllowed(top_frame_origin)) {
    JoinFledgeHistogram(interest_group_api_operation,
                        Status::kJoiningTopFrameBlocked);
    return false;
  }

  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4)) {
    Status status = GetM1FledgeAllowedStatus(top_frame_origin, auction_party);
    JoinFledgeHistogram(interest_group_api_operation, status);
    return IsAllowed(status);
  }

  return IsPrivacySandboxEnabledForContext(top_frame_origin,
                                           auction_party.GetURL());
}

bool PrivacySandboxSettingsImpl::IsSharedStorageAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin,
    content::RenderFrameHost* console_frame) const {
  // Check for attestation on the caller's site.
  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          net::SchemefulSite(accessing_origin),
          PrivacySandboxAttestationsGatedAPI::kSharedStorage);
  if (!IsAllowed(attestation_status)) {
    JoinHistogram(kIsSharedStorageAllowedHistogram, attestation_status);
    if (console_frame) {
      console_frame->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          "Attestation check for Shared Storage on " +
              accessing_origin.Serialize() + " failed.");
    }
    return false;
  }

  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4)) {
    Status status = GetPrivacySandboxAllowedStatus();
    if (IsAllowed(status)) {
      status = GetSiteAccessAllowedStatus(top_frame_origin,
                                          accessing_origin.GetURL());
    }
    JoinHistogram(kIsSharedStorageAllowedHistogram, status);
    return IsAllowed(status);
  }

  // Ensures that Shared Storage is only allowed if both Privacy Sandbox is
  // enabled and full cookie access is enabled for this context.
  return IsPrivacySandboxEnabledForContext(top_frame_origin,
                                           accessing_origin.GetURL());
}

bool PrivacySandboxSettingsImpl::IsSharedStorageSelectURLAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin) const {
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4)) {
    Status status =
        GetM1FledgeAllowedStatus(top_frame_origin, accessing_origin);
    JoinHistogram(kIsSharedStorageSelectURLAllowedHistogram, status);
    return IsAllowed(status);
  }

  return IsSharedStorageAllowed(top_frame_origin, accessing_origin);
}

bool PrivacySandboxSettingsImpl::IsPrivateAggregationAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& reporting_origin) const {
  // Check for attestation on the worklet's site.
  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          net::SchemefulSite(reporting_origin),
          PrivacySandboxAttestationsGatedAPI::kPrivateAggregation);
  if (!IsAllowed(attestation_status)) {
    JoinHistogram(kIsPrivateAggregationAllowedHistogram, attestation_status);
    return false;
  }

  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4)) {
    Status status = GetM1AttributionReportingAllowedStatus(top_frame_origin,
                                                           reporting_origin);
    JoinHistogram(kIsPrivateAggregationAllowedHistogram, status);
    return IsAllowed(status);
  }

  return IsPrivacySandboxEnabledForContext(top_frame_origin,
                                           reporting_origin.GetURL());
}

bool PrivacySandboxSettingsImpl::IsPrivateAggregationDebugModeAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& reporting_origin) const {
  if (!IsPrivateAggregationAllowed(top_frame_origin, reporting_origin)) {
    return false;
  }

  // Third party cookies must also be available for this context. An empty site
  // for cookies is provided so the context is always treated as a third party.
  return cookie_settings_->IsFullCookieAccessAllowed(
      reporting_origin.GetURL(), net::SiteForCookies(), top_frame_origin,
      net::CookieSettingOverrides());
}

bool PrivacySandboxSettingsImpl::IsPrivacySandboxEnabled() const {
  PrivacySandboxSettingsImpl::Status status = GetPrivacySandboxAllowedStatus();
  if (!IsAllowed(status)) {
    return false;
  }

  // For Measurement and Relevance APIs, we explicitly do not require the
  // underlying pref to be enabled if there is a local flag enabling the APIs to
  // allow for local testing.
  if (base::FeatureList::IsEnabled(
          privacy_sandbox::kOverridePrivacySandboxSettingsLocalTesting)) {
    return true;
  }

  return pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabledV2);
}

void PrivacySandboxSettingsImpl::SetAllPrivacySandboxAllowedForTesting() {
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4)) {
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                              true);
    return;
  }

  pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, true);
}

void PrivacySandboxSettingsImpl::SetTopicsBlockedForTesting() {
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4)) {
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, false);
    return;
  }

  pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, false);
}

void PrivacySandboxSettingsImpl::SetPrivacySandboxEnabled(bool enabled) {
  pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, enabled);
}

bool PrivacySandboxSettingsImpl::IsPrivacySandboxRestricted() const {
  return delegate_->IsPrivacySandboxRestricted();
}

bool PrivacySandboxSettingsImpl::IsPrivacySandboxCurrentlyUnrestricted() const {
  return delegate_->IsPrivacySandboxCurrentlyUnrestricted();
}

bool PrivacySandboxSettingsImpl::IsSubjectToM1NoticeRestricted() const {
  return delegate_->IsSubjectToM1NoticeRestricted();
}

bool PrivacySandboxSettingsImpl::IsRestrictedNoticeEnabled() const {
  return privacy_sandbox::kPrivacySandboxSettings4RestrictedNotice.Get();
}

void PrivacySandboxSettingsImpl::OnCookiesCleared() {
  SetTopicsDataAccessibleFromNow();
}

void PrivacySandboxSettingsImpl::OnRelatedWebsiteSetsEnabledPrefChanged() {
  if (!base::FeatureList::IsEnabled(features::kFirstPartySets)) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnFirstPartySetsEnabledChanged(AreRelatedWebsiteSetsEnabled());
  }
}

void PrivacySandboxSettingsImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrivacySandboxSettingsImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PrivacySandboxSettingsImpl::SetDelegateForTesting(
    std::unique_ptr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

bool PrivacySandboxSettingsImpl::IsPrivacySandboxEnabledForContext(
    const absl::optional<url::Origin>& top_frame_origin,
    const GURL& url) const {
  if (!IsPrivacySandboxEnabled()) {
    return false;
  }

  // Third party cookies must also be available for this context. An empty site
  // for cookies is provided so the context is always treated as a third party.
  return cookie_settings_->IsFullCookieAccessAllowed(
      url, net::SiteForCookies(), top_frame_origin,
      net::CookieSettingOverrides());
}

void PrivacySandboxSettingsImpl::SetTopicsDataAccessibleFromNow() const {
  pref_service_->SetTime(prefs::kPrivacySandboxTopicsDataAccessibleSince,
                         base::Time::Now());

  for (auto& observer : observers_) {
    observer.OnTopicsDataAccessibleSinceUpdated();
  }
}

PrivacySandboxSettingsImpl::Status
PrivacySandboxSettingsImpl::GetSiteAccessAllowedStatus(
    const url::Origin& top_frame_origin,
    const GURL& url) const {
  // Relying on |host_content_settings_map_| instead of |cookie_settings_|
  // allows to query whether the site associated with the |url| is allowed to
  // access Site data (aka ContentSettingsType::COOKIES) without considering any
  // 3P cookie blocking setting.
  return content_settings::CookieSettingsBase::IsAllowed(
             host_content_settings_map_->GetContentSetting(
                 url, top_frame_origin.GetURL(), ContentSettingsType::COOKIES))
             ? Status::kAllowed
             : Status::kSiteDataAccessBlocked;
}

PrivacySandboxSettingsImpl::Status
PrivacySandboxSettingsImpl::GetPrivacySandboxAllowedStatus(
    bool should_ignore_restriction /*=false*/) const {
  if (delegate_->IsIncognitoProfile()) {
    return Status::kIncognitoProfile;
  }

  if (IsPrivacySandboxRestricted() && !should_ignore_restriction) {
    return Status::kRestricted;
  }

  return Status::kAllowed;
}

PrivacySandboxSettingsImpl::Status
PrivacySandboxSettingsImpl::GetM1PrivacySandboxApiEnabledStatus(
    const std::string& pref_name) const {
  DCHECK(pref_name == prefs::kPrivacySandboxM1TopicsEnabled ||
         pref_name == prefs::kPrivacySandboxM1FledgeEnabled ||
         pref_name == prefs::kPrivacySandboxM1AdMeasurementEnabled);
  if (delegate_->IsCookieDeprecationExperimentEligible() &&
      features::kCookieDeprecationTestingDisableAdsAPIs.Get()) {
    return Status::kBlockedBy3pcdExperiment;
  }

  bool should_ignore_restriction =
      pref_name == prefs::kPrivacySandboxM1AdMeasurementEnabled &&
      IsRestrictedNoticeEnabled();
  PrivacySandboxSettingsImpl::Status status =
      GetPrivacySandboxAllowedStatus(should_ignore_restriction);
  if (!IsAllowed(status)) {
    return status;
  }

  // For Measurement and Relevance APIs, we explicitly do not require the
  // underlying pref to be enabled if there is a local flag enabling the APIs to
  // allow for local testing.
  if (base::FeatureList::IsEnabled(
          privacy_sandbox::kOverridePrivacySandboxSettingsLocalTesting)) {
    return Status::kAllowed;
  }

  status = (pref_service_->GetBoolean(pref_name)) ? Status::kAllowed
                                                  : Status::kApisDisabled;
  return status;
}

TpcdExperimentEligibility
PrivacySandboxSettingsImpl::GetCookieDeprecationExperimentCurrentEligibility()
    const {
  return delegate_->GetCookieDeprecationExperimentCurrentEligibility();
}

bool PrivacySandboxSettingsImpl::IsCookieDeprecationLabelAllowed() const {
  return delegate_->IsCookieDeprecationLabelAllowed();
}

bool PrivacySandboxSettingsImpl::IsCookieDeprecationLabelAllowedForContext(
    const url::Origin& top_frame_origin,
    const url::Origin& context_origin) const {
  if (!IsCookieDeprecationLabelAllowed()) {
    return false;
  }

  return IsAllowed(
      GetSiteAccessAllowedStatus(top_frame_origin, context_origin.GetURL()));
}

void PrivacySandboxSettingsImpl::OnBlockAllThirdPartyCookiesChanged() {
  if (!base::FeatureList::IsEnabled(features::kFirstPartySets)) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnFirstPartySetsEnabledChanged(AreRelatedWebsiteSetsEnabled());
  }
}

bool PrivacySandboxSettingsImpl::AreRelatedWebsiteSetsEnabled() const {
  // FPS should be on in the 3PCD experiment unless all 3PC are blocked.
  if (tracking_protection_settings_->IsTrackingProtection3pcdEnabled()) {
    return !tracking_protection_settings_->AreAllThirdPartyCookiesBlocked();
  }
  return pref_service_->GetBoolean(
      prefs::kPrivacySandboxRelatedWebsiteSetsEnabled);
}

}  // namespace privacy_sandbox
