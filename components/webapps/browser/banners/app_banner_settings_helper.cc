// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/banners/app_banner_settings_helper.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/permissions_client.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/banners/app_banner_metrics.h"
#include "components/webapps/common/switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace webapps {

namespace {

// Max number of apps (including ServiceWorker based web apps) that a particular
// site may show a banner for.
const size_t kMaxAppsPerSite = 3;

// Default number of days that dismissing or ignoring the banner will prevent it
// being seen again for.
const unsigned int kMinimumBannerBlockedToBannerShown = 90;
const unsigned int kMinimumDaysBetweenBannerShows = 14;

// Default site engagement required to trigger the banner.
const unsigned int kDefaultTotalEngagementToTrigger = 2;

// The number of days in the past that a site should be launched from homescreen
// to be considered recent.
// TODO(dominickn): work out how to unify this with
// WebappDataStorage.wasLaunchedRecently.
const unsigned int kRecentLastLaunchInDays = 10;

// Dictionary keys to use for the events. Must be kept in sync with
// AppBannerEvent.
constexpr const char* kBannerEventKeys[] = {
    // clang-format off
    "couldShowBannerEvents",
    "didShowBannerEvent",
    "didBlockBannerEvent",
    "didAddToHomescreenEvent",
    "couldShowAmbientBadgeEvent",
    // clang-format on
};

// Keys to use when querying the variations params.
const char kBannerParamsKey[] = "AppBannerTriggering";
const char kBannerParamsEngagementTotalKey[] = "site_engagement_total";
const char kBannerParamsDaysAfterBannerDismissedKey[] = "days_after_dismiss";
const char kBannerParamsDaysAfterBannerIgnoredKey[] = "days_after_ignore";

// Total engagement score required before a banner will actually be triggered.
double gTotalEngagementToTrigger = kDefaultTotalEngagementToTrigger;

unsigned int gDaysAfterDismissedToShow = kMinimumBannerBlockedToBannerShown;
unsigned int gDaysAfterIgnoredToShow = kMinimumDaysBetweenBannerShows;

base::Value::Dict GetOriginAppBannerData(HostContentSettingsMap* settings,
                                         const GURL& origin_url) {
  if (!settings)
    return base::Value::Dict();

  base::Value dict = settings->GetWebsiteSetting(
      origin_url, origin_url, ContentSettingsType::APP_BANNER, nullptr);

  if (!dict.is_dict())
    return base::Value::Dict();

  return std::move(dict.GetDict());
}

class AppPrefs {
 public:
  AppPrefs(content::WebContents* web_contents,
           const GURL& origin,
           const std::string& package_name_or_start_url)
      : origin_(origin) {
    content::BrowserContext* browser_context =
        web_contents->GetBrowserContext();
    if (browser_context->IsOffTheRecord() || !origin.is_valid())
      return;

    settings_ =
        permissions::PermissionsClient::Get()->GetSettingsMap(browser_context);
    origin_dict_ = GetOriginAppBannerData(settings_, origin);
    dict_ = origin_dict_.FindDict(package_name_or_start_url);
    if (!dict_) {
      // Don't allow more than kMaxAppsPerSite dictionaries.
      if (origin_dict_.size() < kMaxAppsPerSite) {
        dict_ =
            origin_dict_.Set(package_name_or_start_url, base::Value::Dict())
                ->GetIfDict();
      }
    }
  }

  HostContentSettingsMap* settings() { return settings_; }
  base::Value::Dict* dict() { return dict_; }

  void Save() {
    DCHECK(dict_);
    dict_ = nullptr;
    settings_->SetWebsiteSettingDefaultScope(
        *origin_, GURL(), ContentSettingsType::APP_BANNER,
        base::Value(std::move(origin_dict_)));
  }

 private:
  const raw_ref<const GURL> origin_;
  raw_ptr<HostContentSettingsMap> settings_ = nullptr;
  base::Value::Dict origin_dict_;
  raw_ptr<base::Value::Dict> dict_ = nullptr;
};

// Queries variations for the number of days which dismissing and ignoring the
// banner should prevent a banner from showing.
void UpdateDaysBetweenShowing() {
  std::string dismiss_param = base::GetFieldTrialParamValue(
      kBannerParamsKey, kBannerParamsDaysAfterBannerDismissedKey);
  std::string ignore_param = base::GetFieldTrialParamValue(
      kBannerParamsKey, kBannerParamsDaysAfterBannerIgnoredKey);

  if (!dismiss_param.empty() && !ignore_param.empty()) {
    unsigned int dismiss_days = 0;
    unsigned int ignore_days = 0;

    if (base::StringToUint(dismiss_param, &dismiss_days) &&
        base::StringToUint(ignore_param, &ignore_days)) {
      AppBannerSettingsHelper::SetDaysAfterDismissAndIgnoreToTrigger(
          dismiss_days, ignore_days);
    }
  }
}

// Queries variations for the maximum site engagement score required to trigger
// the banner showing.
void UpdateSiteEngagementToTrigger() {
  std::string total_param = base::GetFieldTrialParamValue(
      kBannerParamsKey, kBannerParamsEngagementTotalKey);

  if (!total_param.empty()) {
    double total_engagement = -1;

    if (base::StringToDouble(total_param, &total_engagement) &&
        total_engagement >= 0) {
      AppBannerSettingsHelper::SetTotalEngagementToTrigger(total_engagement);
    }
  }
}

// Reports whether |event| was recorded within the |period| up until |now|.
// If we get nullopt, we cannot store any more values for |origin_url|.
// Conservatively assume we did block a banner in this case.
absl::optional<bool> WasEventWithinPeriod(
    AppBannerSettingsHelper::AppBannerEvent event,
    base::TimeDelta period,
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& package_name_or_start_url,
    base::Time now) {
  absl::optional<base::Time> event_time =
      AppBannerSettingsHelper::GetSingleBannerEvent(
          web_contents, origin_url, package_name_or_start_url, event);

  if (!event_time)
    return absl::nullopt;

  // Null times are in the distant past, so the delta between real times and
  // null events will always be greater than the limits.
  return (now - *event_time < period);
}

// Dictionary of time information for how long to wait before showing the
// "Install" text slide animation again.
// Data format: {"last_shown": timestamp, "delay": duration}
constexpr char kNextInstallTextAnimation[] = "next_install_text_animation";
constexpr char kLastShownKey[] = "last_shown";
constexpr char kDelayKey[] = "delay";

struct NextInstallTextAnimation {
  base::Time last_shown;
  base::TimeDelta delay;

  static absl::optional<NextInstallTextAnimation> Get(
      content::WebContents* web_contents,
      const GURL& scope);

  base::Time Time() const { return last_shown + delay; }

  void RecordToPrefs(content::WebContents* web_contents,
                     const GURL& scope) const;
};

absl::optional<NextInstallTextAnimation> NextInstallTextAnimation::Get(
    content::WebContents* web_contents,
    const GURL& scope) {
  AppPrefs app_prefs(web_contents, scope, scope.spec());
  if (!app_prefs.dict())
    return NextInstallTextAnimation{base::Time::Max(), base::TimeDelta::Max()};

  const base::Value::Dict* next_dict =
      app_prefs.dict()->FindDict(kNextInstallTextAnimation);
  if (!next_dict)
    return absl::nullopt;

  absl::optional<base::Time> last_shown =
      base::ValueToTime(next_dict->Find(kLastShownKey));
  if (!last_shown)
    return absl::nullopt;

  absl::optional<base::TimeDelta> delay =
      base::ValueToTimeDelta(next_dict->Find(kDelayKey));
  if (!delay)
    return absl::nullopt;

  return NextInstallTextAnimation{*last_shown, *delay};
}

void NextInstallTextAnimation::RecordToPrefs(content::WebContents* web_contents,
                                             const GURL& scope) const {
  AppPrefs app_prefs(web_contents, scope, scope.spec());
  if (!app_prefs.dict())
    return;

  base::Value::Dict next_dict;
  next_dict.Set(kLastShownKey, base::TimeToValue(last_shown));
  next_dict.Set(kDelayKey, base::TimeDeltaToValue(delay));
  app_prefs.dict()->Set(kNextInstallTextAnimation, std::move(next_dict));
  app_prefs.Save();
}

}  // namespace

// Key to store instant apps events.
const char AppBannerSettingsHelper::kInstantAppsKey[] = "instantapps";

void AppBannerSettingsHelper::ClearHistoryForURLs(
    content::BrowserContext* browser_context,
    const std::set<GURL>& origin_urls) {
  HostContentSettingsMap* settings =
      permissions::PermissionsClient::Get()->GetSettingsMap(browser_context);
  for (const GURL& origin_url : origin_urls) {
    settings->SetWebsiteSettingDefaultScope(
        origin_url, GURL(), ContentSettingsType::APP_BANNER, base::Value());
    settings->FlushLossyWebsiteSettings();
  }
}

void AppBannerSettingsHelper::RecordBannerInstallEvent(
    content::WebContents* web_contents,
    const std::string& package_name_or_start_url) {
  TrackInstallEvent(INSTALL_EVENT_WEB_APP_INSTALLED);

  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents, web_contents->GetLastCommittedURL(),
      package_name_or_start_url,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
      AppBannerManager::GetCurrentTime());
}

void AppBannerSettingsHelper::RecordBannerDismissEvent(
    content::WebContents* web_contents,
    const std::string& package_name_or_start_url) {
  TrackDismissEvent(DISMISS_EVENT_CLOSE_BUTTON);

  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents, web_contents->GetLastCommittedURL(),
      package_name_or_start_url,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_BLOCK,
      AppBannerManager::GetCurrentTime());
}

void AppBannerSettingsHelper::RecordBannerEvent(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& package_name_or_start_url,
    AppBannerEvent event,
    base::Time time) {
  AppPrefs app_prefs(web_contents, origin_url, package_name_or_start_url);
  if (!app_prefs.dict())
    return;

  // Dates are stored in their raw form (i.e. not local dates) to be resilient
  // to time zone changes.
  const char* event_key = kBannerEventKeys[event];

  if (event == APP_BANNER_EVENT_COULD_SHOW) {
    // Do not overwrite a could show event, as this is used for metrics.
    if (app_prefs.dict()->contains(event_key))
      return;
  }
  app_prefs.dict()->Set(
      event_key, base::Value(static_cast<double>(time.ToInternalValue())));

  app_prefs.Save();

  // App banner content settings are lossy, meaning they will not cause the
  // prefs to become dirty. This is fine for most events, as if they are lost it
  // just means the user will have to engage a little bit more. However the
  // DID_ADD_TO_HOMESCREEN event should always be recorded to prevent
  // spamminess.
  if (event == APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN)
    app_prefs.settings()->FlushLossyWebsiteSettings();
}

bool AppBannerSettingsHelper::HasBeenInstalled(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& package_name_or_start_url) {
  absl::optional<base::Time> added_time =
      GetSingleBannerEvent(web_contents, origin_url, package_name_or_start_url,
                           APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN);

  return added_time && !added_time->is_null();
}

bool AppBannerSettingsHelper::WasBannerRecentlyBlocked(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& package_name_or_start_url,
    base::Time now) {
  DCHECK(!package_name_or_start_url.empty());

  absl::optional<bool> in_period = WasEventWithinPeriod(
      APP_BANNER_EVENT_DID_BLOCK, base::Days(gDaysAfterDismissedToShow),
      web_contents, origin_url, package_name_or_start_url, now);
  return in_period.value_or(true);
}

bool AppBannerSettingsHelper::WasBannerRecentlyIgnored(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& package_name_or_start_url,
    base::Time now) {
  DCHECK(!package_name_or_start_url.empty());

  absl::optional<bool> in_period = WasEventWithinPeriod(
      APP_BANNER_EVENT_DID_SHOW, base::Days(gDaysAfterIgnoredToShow),
      web_contents, origin_url, package_name_or_start_url, now);

  return in_period.value_or(true);
}

absl::optional<base::Time> AppBannerSettingsHelper::GetSingleBannerEvent(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& package_name_or_start_url,
    AppBannerEvent event) {
  DCHECK(event < APP_BANNER_EVENT_NUM_EVENTS);

  AppPrefs app_prefs(web_contents, origin_url, package_name_or_start_url);
  if (!app_prefs.dict())
    return absl::nullopt;

  absl::optional<double> internal_time =
      app_prefs.dict()->FindDouble(kBannerEventKeys[event]);
  return internal_time ? base::Time::FromInternalValue(internal_time.value())
                       : base::Time();
}

bool AppBannerSettingsHelper::HasSufficientEngagement(double total_engagement) {
  return (base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kBypassAppBannerEngagementChecks)) ||
         (total_engagement >= gTotalEngagementToTrigger);
}

bool AppBannerSettingsHelper::WasLaunchedRecently(
    content::BrowserContext* browser_context,
    const GURL& origin_url,
    base::Time now) {
  HostContentSettingsMap* settings =
      permissions::PermissionsClient::Get()->GetSettingsMap(browser_context);
  base::Value::Dict origin_dict = GetOriginAppBannerData(settings, origin_url);

  // Iterate over everything in the content setting, which should be a set of
  // dictionaries per app path. If we find one that has been added to
  // homescreen recently, return true.
  base::TimeDelta recent_last_launch_in_days =
      base::Days(kRecentLastLaunchInDays);
  for (auto [key, value] : origin_dict) {
    if (value.is_dict()) {
      // TODO(https://crbug.com/1338016): Delete stored Instant App data.
      if (key == kInstantAppsKey)
        continue;

      absl::optional<double> internal_time = value.GetDict().FindDouble(
          kBannerEventKeys[APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN]);
      if (!internal_time)
        continue;

      if ((now - base::Time::FromInternalValue(*internal_time)) <=
          recent_last_launch_in_days) {
        return true;
      }
    }
  }

  return false;
}

void AppBannerSettingsHelper::SetDaysAfterDismissAndIgnoreToTrigger(
    unsigned int dismiss_days,
    unsigned int ignore_days) {
  gDaysAfterDismissedToShow = dismiss_days;
  gDaysAfterIgnoredToShow = ignore_days;
}

void AppBannerSettingsHelper::SetTotalEngagementToTrigger(
    double total_engagement) {
  gTotalEngagementToTrigger = total_engagement;
}

void AppBannerSettingsHelper::SetDefaultParameters() {
  SetTotalEngagementToTrigger(kDefaultTotalEngagementToTrigger);
}

void AppBannerSettingsHelper::UpdateFromFieldTrial() {
  // If we are using the site engagement score, only extract the total
  // engagement to trigger from the params variations.
  UpdateDaysBetweenShowing();
  UpdateSiteEngagementToTrigger();
}

bool AppBannerSettingsHelper::CanShowInstallTextAnimation(
    content::WebContents* web_contents,
    const GURL& scope) {
  absl::optional<NextInstallTextAnimation> next_prompt =
      NextInstallTextAnimation::Get(web_contents, scope);

  if (!next_prompt)
    return true;

  return AppBannerManager::GetCurrentTime() >= next_prompt->Time();
}

void AppBannerSettingsHelper::RecordInstallTextAnimationShown(
    content::WebContents* web_contents,
    const GURL& scope) {
  DCHECK(scope.is_valid());

  constexpr base::TimeDelta kInitialAnimationSuppressionPeriod = base::Days(1);
  constexpr base::TimeDelta kMaxAnimationSuppressionPeriod = base::Days(90);
  constexpr double kExponentialBackoffFactor = 2;

  NextInstallTextAnimation next_prompt = {AppBannerManager::GetCurrentTime(),
                                          kInitialAnimationSuppressionPeriod};

  absl::optional<NextInstallTextAnimation> last_prompt =
      NextInstallTextAnimation::Get(web_contents, scope);
  if (last_prompt) {
    next_prompt.delay =
        std::min(kMaxAnimationSuppressionPeriod,
                 last_prompt->delay * kExponentialBackoffFactor);
  }

  next_prompt.RecordToPrefs(web_contents, scope);
}

AppBannerSettingsHelper::ScopedTriggerSettings::ScopedTriggerSettings(
    unsigned int dismiss_days,
    unsigned int ignore_days) {
  old_dismiss_ = gDaysAfterDismissedToShow;
  old_ignore_ = gDaysAfterIgnoredToShow;
  gDaysAfterDismissedToShow = dismiss_days;
  gDaysAfterIgnoredToShow = ignore_days;
}

AppBannerSettingsHelper::ScopedTriggerSettings::~ScopedTriggerSettings() {
  gDaysAfterDismissedToShow = old_dismiss_;
  gDaysAfterIgnoredToShow = old_ignore_;
}

}  // namespace webapps
