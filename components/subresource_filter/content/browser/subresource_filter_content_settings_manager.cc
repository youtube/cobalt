// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace subresource_filter {

namespace {

// Key into the website setting dict for the smart UI.
const char kInfobarLastShownTimeKey[] = "InfobarLastShownTime";
const char kActivatedKey[] = "Activated";
const char kNonRenewingExpiryTime[] = "NonRenewingExpiryTime";

bool ShouldUseSmartUI() {
#if BUILDFLAG(IS_ANDROID)
  return true;
#else
  return false;
#endif
}

}  // namespace

constexpr base::TimeDelta
    SubresourceFilterContentSettingsManager::kDelayBeforeShowingInfobarAgain;

constexpr base::TimeDelta
    SubresourceFilterContentSettingsManager::kMaxPersistMetadataDuration;

SubresourceFilterContentSettingsManager::
    SubresourceFilterContentSettingsManager(
        HostContentSettingsMap* settings_map)
    : settings_map_(settings_map),
      clock_(std::make_unique<base::DefaultClock>(base::DefaultClock())),
      should_use_smart_ui_(ShouldUseSmartUI()) {
  DCHECK(settings_map_);
}

SubresourceFilterContentSettingsManager::
    ~SubresourceFilterContentSettingsManager() = default;

ContentSetting SubresourceFilterContentSettingsManager::GetSitePermission(
    const GURL& url) const {
  return settings_map_->GetContentSetting(url, GURL(),
                                          ContentSettingsType::ADS);
}

void SubresourceFilterContentSettingsManager::AllowlistSite(const GURL& url) {
  settings_map_->SetContentSettingDefaultScope(
      url, GURL(), ContentSettingsType::ADS, CONTENT_SETTING_ALLOW);
}

void SubresourceFilterContentSettingsManager::OnDidShowUI(const GURL& url) {
  absl::optional<base::Value::Dict> dict = GetSiteMetadata(url);
  if (!dict)
    dict = CreateMetadataDictWithActivation(true /* is_activated */);

  double now = clock_->Now().ToDoubleT();
  dict->Set(kInfobarLastShownTimeKey, now);
  SetSiteMetadata(url, std::move(dict));
}

bool SubresourceFilterContentSettingsManager::ShouldShowUIForSite(
    const GURL& url) const {
  if (!should_use_smart_ui())
    return true;

  absl::optional<base::Value::Dict> dict = GetSiteMetadata(url);
  if (!dict)
    return true;

  if (absl::optional<double> last_shown_time =
          dict->FindDouble(kInfobarLastShownTimeKey)) {
    base::Time last_shown = base::Time::FromDoubleT(*last_shown_time);
    if (clock_->Now() - last_shown < kDelayBeforeShowingInfobarAgain)
      return false;
  }
  return true;
}

void SubresourceFilterContentSettingsManager::SetSiteMetadataBasedOnActivation(
    const GURL& url,
    bool is_activated,
    ActivationSource activation_source,
    absl::optional<base::Value::Dict> additional_data) {
  absl::optional<base::Value::Dict> dict = GetSiteMetadata(url);

  if (!is_activated &&
      ShouldDeleteDataWithNoActivation(dict, activation_source)) {
    // If we are clearing metadata, there should be no additional_data dict.
    DCHECK(!additional_data);
    SetSiteMetadata(url, absl::nullopt);
    return;
  }

  // Do not create new metadata if it exists already, it could clobber
  // existing data.
  if (!dict)
    dict = CreateMetadataDictWithActivation(is_activated /* is_activated */);
  else
    dict->Set(kActivatedKey, is_activated);

  if (additional_data)
    dict->Merge(std::move(*additional_data));

  // Ads intervention metadata should not be deleted by changes in activation
  // during the metrics collection period (kMaxPersistMetadataDuration).
  // Setting the key kNonRenewingExpiryTime enforces this behavior in
  // SetSiteMetadata.
  if (activation_source == ActivationSource::kAdsIntervention) {
    // If we have an expiry time set, then we are already tracking
    // an ads intervention. Since we should not be able to trigger a new ads
    // intervention once we should be blocking ads, do not change the expiry
    // time or overwrite existing ads intervention metadata,
    if (dict->FindDouble(kNonRenewingExpiryTime))
      return;
    double expiry_time =
        (clock_->Now() + kMaxPersistMetadataDuration).ToDoubleT();
    dict->Set(kNonRenewingExpiryTime, expiry_time);
  }

  SetSiteMetadata(url, std::move(dict));
}

absl::optional<base::Value::Dict>
SubresourceFilterContentSettingsManager::GetSiteMetadata(
    const GURL& url) const {
  base::Value value = settings_map_->GetWebsiteSetting(
      url, GURL(), ContentSettingsType::ADS_DATA, nullptr);
  if (!value.is_dict())
    return absl::nullopt;

  return std::move(value).TakeDict();
}

void SubresourceFilterContentSettingsManager::SetSiteMetadataForTesting(
    const GURL& url,
    absl::optional<base::Value::Dict> dict) {
  SetSiteMetadata(url, std::move(dict));
}

void SubresourceFilterContentSettingsManager::SetSiteMetadata(
    const GURL& url,
    absl::optional<base::Value::Dict> dict) {
  if (url.is_empty())
    return;

  // Metadata expires after kMaxPersistMetadataDuration by default. If
  // kNonRenewingExpiryTime was previously set, then we are storing ads
  // intervention metadata and should not override the expiry time that
  // was previously set.
  base::Time expiry_time = base::Time::Now() + kMaxPersistMetadataDuration;
  if (dict && dict->Find(kNonRenewingExpiryTime)) {
    absl::optional<double> metadata_expiry_time =
        dict->FindDouble(kNonRenewingExpiryTime);
    DCHECK(metadata_expiry_time);
    expiry_time = base::Time::FromDoubleT(*metadata_expiry_time);
  }

  content_settings::ContentSettingConstraints constraints = {expiry_time};
  settings_map_->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::ADS_DATA,
      dict ? base::Value(std::move(*dict)) : base::Value(), constraints);
}

base::Value::Dict
SubresourceFilterContentSettingsManager::CreateMetadataDictWithActivation(
    bool is_activated) {
  base::Value::Dict dict;
  dict.Set(kActivatedKey, is_activated);

  return dict;
}

bool SubresourceFilterContentSettingsManager::ShouldDeleteDataWithNoActivation(
    const absl::optional<base::Value::Dict>& dict,
    ActivationSource activation_source) {
  // For the ads intervention dry run experiment we want to make sure that
  // non activated pages get properly tagged for metrics collection. Don't
  // delete them from storage until their associated intervention _would have_
  // expired.
  if (activation_source != ActivationSource::kSafeBrowsing)
    return false;

  if (!dict)
    return true;

  absl::optional<double> metadata_expiry_time =
      dict->FindDouble(kNonRenewingExpiryTime);

  if (!metadata_expiry_time)
    return true;

  base::Time expiry_time = base::Time::FromDoubleT(*metadata_expiry_time);
  return clock_->Now() > expiry_time;
}

bool SubresourceFilterContentSettingsManager::GetSiteActivationFromMetadata(
    const GURL& url) {
  absl::optional<base::Value::Dict> dict = GetSiteMetadata(url);

  // If there is no dict, this is metadata V1, absence of metadata
  // implies no activation.
  if (!dict)
    return false;

  absl::optional<bool> site_activation_status = dict->FindBool(kActivatedKey);

  // If there is no explicit site activation status, it is metadata V1:
  // use the presence of metadata as indicative of the site activation.
  // Otherwise it is metadata V2, we return the activation stored in
  // kActivatedKey.
  return !site_activation_status || *site_activation_status;
}

void SubresourceFilterContentSettingsManager::ClearSiteMetadata(
    const GURL& url) {
  SetSiteMetadata(url, absl::nullopt);
}

void SubresourceFilterContentSettingsManager::ClearMetadataForAllSites() {
  settings_map_->ClearSettingsForOneType(ContentSettingsType::ADS_DATA);
}

}  // namespace subresource_filter
