// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/ntp_user_data_logger.h"

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/search/ntp_user_data_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/ntp_tiles/metrics.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"

namespace {

constexpr char kUIEventCategory[] = "ui";

// Logs CustomizedShortcutSettings on the NTP.
void LogCustomizedShortcutSettings(bool using_most_visited, bool is_visible) {
  CustomizedShortcutSettings setting;
  if (is_visible && using_most_visited) {
    setting =
        CustomizedShortcutSettings::CUSTOMIZED_SHORTCUT_SETTINGS_MOST_VISITED;
  } else if (is_visible && !using_most_visited) {
    setting =
        CustomizedShortcutSettings::CUSTOMIZED_SHORTCUT_SETTINGS_CUSTOM_LINKS;
  } else {
    setting = CustomizedShortcutSettings::CUSTOMIZED_SHORTCUT_SETTINGS_HIDDEN;
  }

  UMA_HISTOGRAM_ENUMERATION("NewTabPage.CustomizedShortcuts", setting);
}

// Converts |NTPLoggingEventType| to a |CustomizedFeature|.
CustomizedFeature LoggingEventToCustomizedFeature(NTPLoggingEventType event) {
  switch (event) {
    case NTP_BACKGROUND_CUSTOMIZED:
      return CustomizedFeature::CUSTOMIZED_FEATURE_BACKGROUND;
    case NTP_SHORTCUT_CUSTOMIZED:
      return CustomizedFeature::CUSTOMIZED_FEATURE_SHORTCUT;
    default:
      break;
  }

  NOTREACHED();
  return CustomizedFeature::CUSTOMIZED_FEATURE_BACKGROUND;
}

// Converts |NTPLoggingEventType| to a |CustomizeChromeBackgroundAction|.
CustomizeChromeBackgroundAction LoggingEventToCustomizeChromeBackgroundAction(
    NTPLoggingEventType event) {
  switch (event) {
    case NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_COLLECTION:
      return CustomizeChromeBackgroundAction::
          CUSTOMIZE_CHROME_BACKGROUND_ACTION_SELECT_COLLECTION;
    case NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_IMAGE:
      return CustomizeChromeBackgroundAction::
          CUSTOMIZE_CHROME_BACKGROUND_ACTION_SELECT_IMAGE;
    case NTP_CUSTOMIZE_CHROME_BACKGROUND_CANCEL:
      return CustomizeChromeBackgroundAction::
          CUSTOMIZE_CHROME_BACKGROUND_ACTION_CANCEL;
    case NTP_CUSTOMIZE_CHROME_BACKGROUND_DONE:
      return CustomizeChromeBackgroundAction::
          CUSTOMIZE_CHROME_BACKGROUND_ACTION_DONE;
    default:
      break;
  }

  NOTREACHED();
  return CustomizeChromeBackgroundAction::
      CUSTOMIZE_CHROME_BACKGROUND_ACTION_SELECT_COLLECTION;
}

// Converts |NTPLoggingEventType| to a |CustomizeLocalImageBackgroundAction|.
CustomizeLocalImageBackgroundAction
LoggingEventToCustomizeLocalImageBackgroundAction(NTPLoggingEventType event) {
  switch (event) {
    case NTP_CUSTOMIZE_LOCAL_IMAGE_CANCEL:
      return CustomizeLocalImageBackgroundAction::
          CUSTOMIZE_LOCAL_IMAGE_BACKGROUND_ACTION_CANCEL;
    case NTP_CUSTOMIZE_LOCAL_IMAGE_DONE:
      return CustomizeLocalImageBackgroundAction::
          CUSTOMIZE_LOCAL_IMAGE_BACKGROUND_ACTION_DONE;
    default:
      break;
  }

  NOTREACHED();
  return CustomizeLocalImageBackgroundAction::
      CUSTOMIZE_LOCAL_IMAGE_BACKGROUND_ACTION_CANCEL;
}

// Converts |NTPLoggingEventType| to a |CustomizeShortcutAction|.
CustomizeShortcutAction LoggingEventToCustomizeShortcutAction(
    NTPLoggingEventType event) {
  switch (event) {
    case NTP_CUSTOMIZE_SHORTCUT_ADD:
      return CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_ADD;
    case NTP_CUSTOMIZE_SHORTCUT_UPDATE:
      return CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_UPDATE;
    case NTP_CUSTOMIZE_SHORTCUT_REMOVE:
      return CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_REMOVE;
    case NTP_CUSTOMIZE_SHORTCUT_CANCEL:
      return CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_CANCEL;
    case NTP_CUSTOMIZE_SHORTCUT_DONE:
      return CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_DONE;
    case NTP_CUSTOMIZE_SHORTCUT_UNDO:
      return CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_UNDO;
    case NTP_CUSTOMIZE_SHORTCUT_RESTORE_ALL:
      return CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_RESTORE_ALL;
    case NTP_CUSTOMIZE_SHORTCUT_TOGGLE_TYPE:
      return CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_TOGGLE_TYPE;
    case NTP_CUSTOMIZE_SHORTCUT_TOGGLE_VISIBILITY:
      return CustomizeShortcutAction::
          CUSTOMIZE_SHORTCUT_ACTION_TOGGLE_VISIBILITY;
    default:
      break;
  }

  NOTREACHED();
  return CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_REMOVE;
}

// Converts a richer picker background related |NTPLoggingEventType|
// to the corresponding UserAction string.
const char* LoggingEventToBackgroundUserActionName(NTPLoggingEventType event) {
  switch (event) {
    case NTP_BACKGROUND_UPLOAD_FROM_DEVICE:
      return "NTPRicherPicker.Backgrounds.UploadClicked";
    case NTP_BACKGROUND_OPEN_COLLECTION:
      return "NTPRicherPicker.Backgrounds.CollectionClicked";
    case NTP_BACKGROUND_SELECT_IMAGE:
      return "NTPRicherPicker.Backgrounds.BackgroundSelected";
    case NTP_BACKGROUND_IMAGE_SET:
      return "NTPRicherPicker.Backgrounds.BackgroundSet";
    case NTP_BACKGROUND_BACK_CLICK:
      return "NTPRicherPicker.Backgrounds.BackClicked";
    case NTP_BACKGROUND_DEFAULT_SELECTED:
      return "NTPRicherPicker.Backgrounds.DefaultSelected";
    case NTP_BACKGROUND_UPLOAD_CANCEL:
      return "NTPRicherPicker.Backgrounds.UploadCanceled";
    case NTP_BACKGROUND_UPLOAD_DONE:
      return "NTPRicherPicker.Backgrounds.UploadConfirmed";
    case NTP_BACKGROUND_IMAGE_RESET:
      return "NTPRicherPicker.Backgrounds.BackgroundReset";
    case NTP_BACKGROUND_REFRESH_TOGGLE_CLICKED:
      return "NTPRicherPicker.Backgrounds.RefreshToggleClicked";
    case NTP_BACKGROUND_DAILY_REFRESH_ENABLED:
      return "NTPRicherPicker.Backgrounds.DailyRefreshEnabled";
    default:
      NOTREACHED();
      return nullptr;
  }
}

// Converts a richer picker menu |NTPLoggingEventType| to the corresponding
// UserAction string.
const char* LoggingEventToMenuUserActionName(NTPLoggingEventType event) {
  switch (event) {
    case NTP_CUSTOMIZATION_MENU_OPENED:
      return "NTPRicherPicker.Opened";
    case NTP_CUSTOMIZATION_MENU_CANCEL:
      return "NTPRicherPicker.CancelClicked";
    case NTP_CUSTOMIZATION_MENU_DONE:
      return "NTPRicherPicker.DoneClicked";
    default:
      NOTREACHED();
      return nullptr;
  }
}

// Converts a richer picker shortcut related |NTPLoggingEventType| to the
// corresponding UserAction string.
const char* LoggingEventToShortcutUserActionName(NTPLoggingEventType event) {
  switch (event) {
    case NTP_CUSTOMIZE_SHORTCUT_CUSTOM_LINKS_CLICKED:
      return "NTPRicherPicker.Shortcuts.CustomLinksClicked";
    case NTP_CUSTOMIZE_SHORTCUT_MOST_VISITED_CLICKED:
      return "NTPRicherPicker.Shortcuts.MostVisitedClicked";
    case NTP_CUSTOMIZE_SHORTCUT_VISIBILITY_TOGGLE_CLICKED:
      return "NTPRicherPicker.Shortcuts.VisibilityToggleClicked";
    default:
      NOTREACHED();
      return nullptr;
  }
}

// This enum must match the numbering for NewTabPageLogoShown in enums.xml.
// Do not reorder or remove items, and only add new items before
// LOGO_IMPRESSION_TYPE_MAX.
enum LogoImpressionType {
  // Static Doodle image.
  LOGO_IMPRESSION_TYPE_STATIC = 0,
  // Call-to-action Doodle image.
  LOGO_IMPRESSION_TYPE_CTA = 1,

  LOGO_IMPRESSION_TYPE_MAX
};

// This enum must match the numbering for NewTabPageLogoClick in enums.xml.
// Do not reorder or remove items, and only add new items before
// LOGO_CLICK_TYPE_MAX.
enum LogoClickType {
  // Static Doodle image.
  LOGO_CLICK_TYPE_STATIC = 0,
  // Call-to-action Doodle image.
  LOGO_CLICK_TYPE_CTA = 1,
  // Animated Doodle image.
  LOGO_CLICK_TYPE_ANIMATED = 2,

  LOGO_CLICK_TYPE_MAX
};

// Converts |NTPLoggingEventType| to a |LogoClickType|, if the value
// is an error value. Otherwise, |LOGO_CLICK_TYPE_MAX| is returned.
LogoClickType LoggingEventToLogoClick(NTPLoggingEventType event) {
  switch (event) {
    case NTP_STATIC_LOGO_CLICKED:
      return LOGO_CLICK_TYPE_STATIC;
    case NTP_CTA_LOGO_CLICKED:
      return LOGO_CLICK_TYPE_CTA;
    case NTP_ANIMATED_LOGO_CLICKED:
      return LOGO_CLICK_TYPE_ANIMATED;
    default:
      NOTREACHED();
      return LOGO_CLICK_TYPE_MAX;
  }
}

}  // namespace

// Helper macro to log a load time to UMA. There's no good reason why we don't
// use one of the standard UMA_HISTORAM_*_TIMES macros, but all their ranges are
// different, and it's not worth changing all the existing histograms.
#define UMA_HISTOGRAM_LOAD_TIME(name, sample)                     \
  UMA_HISTOGRAM_CUSTOM_TIMES(name, sample, base::Milliseconds(1), \
                             base::Seconds(60), 100)

NTPUserDataLogger::NTPUserDataLogger(Profile* profile,
                                     const GURL& ntp_url,
                                     base::Time ntp_navigation_start_time)
    : during_startup_(!AfterStartupTaskUtils::IsBrowserStartupComplete()),
      ntp_url_(ntp_url),
      profile_(profile),
      // TODO(https://crbug.com/1280310): Migrate NTP navigation startup time
      // from base::Time to base::TimeTicks to avoid time glitches.
      ntp_navigation_start_time_(
          base::TimeTicks::UnixEpoch() +
          (ntp_navigation_start_time - base::Time::UnixEpoch())) {}

NTPUserDataLogger::~NTPUserDataLogger() = default;

// static
void NTPUserDataLogger::LogOneGoogleBarFetchDuration(
    bool success,
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.OneGoogleBar.RequestLatency",
                             duration);
  if (success) {
    UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.OneGoogleBar.RequestLatency.Success",
                               duration);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.OneGoogleBar.RequestLatency.Failure",
                               duration);
  }
}

void NTPUserDataLogger::LogEvent(NTPLoggingEventType event,
                                 base::TimeDelta time) {
  if (!DefaultSearchProviderIsGoogle()) {
    return;
  }

  switch (event) {
    case NTP_STATIC_LOGO_SHOWN_FROM_CACHE:
      RecordDoodleImpression(time, /*is_cta=*/false, /*from_cache=*/true);
      break;
    case NTP_STATIC_LOGO_SHOWN_FRESH:
      RecordDoodleImpression(time, /*is_cta=*/false, /*from_cache=*/false);
      break;
    case NTP_CTA_LOGO_SHOWN_FROM_CACHE:
      RecordDoodleImpression(time, /*is_cta=*/true, /*from_cache=*/true);
      break;
    case NTP_CTA_LOGO_SHOWN_FRESH:
      RecordDoodleImpression(time, /*is_cta=*/true, /*from_cache=*/false);
      break;
    case NTP_STATIC_LOGO_CLICKED:
    case NTP_CTA_LOGO_CLICKED:
    case NTP_ANIMATED_LOGO_CLICKED:
      UMA_HISTOGRAM_ENUMERATION("NewTabPage.LogoClick",
                                LoggingEventToLogoClick(event),
                                LOGO_CLICK_TYPE_MAX);
      break;
    case NTP_ONE_GOOGLE_BAR_SHOWN:
      UMA_HISTOGRAM_LOAD_TIME("NewTabPage.OneGoogleBar.ShownTime", time);
      EmitNtpTraceEvent("NewTabPage.OneGoogleBar.ShownTime", time);

      break;
    case NTP_BACKGROUND_CUSTOMIZED:
    case NTP_SHORTCUT_CUSTOMIZED:
      UMA_HISTOGRAM_ENUMERATION("NewTabPage.Customized",
                                LoggingEventToCustomizedFeature(event));
      break;
    case NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_COLLECTION:
    case NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_IMAGE:
    case NTP_CUSTOMIZE_CHROME_BACKGROUND_CANCEL:
    case NTP_CUSTOMIZE_CHROME_BACKGROUND_DONE:
      UMA_HISTOGRAM_ENUMERATION(
          "NewTabPage.CustomizeChromeBackgroundAction",
          LoggingEventToCustomizeChromeBackgroundAction(event));
      break;
    case NTP_CUSTOMIZE_LOCAL_IMAGE_CANCEL:
    case NTP_CUSTOMIZE_LOCAL_IMAGE_DONE:
      UMA_HISTOGRAM_ENUMERATION(
          "NewTabPage.CustomizeLocalImageBackgroundAction",
          LoggingEventToCustomizeLocalImageBackgroundAction(event));
      break;
    case NTP_CUSTOMIZE_SHORTCUT_ADD:
    case NTP_CUSTOMIZE_SHORTCUT_UPDATE:
    case NTP_CUSTOMIZE_SHORTCUT_REMOVE:
    case NTP_CUSTOMIZE_SHORTCUT_CANCEL:
    case NTP_CUSTOMIZE_SHORTCUT_DONE:
    case NTP_CUSTOMIZE_SHORTCUT_UNDO:
    case NTP_CUSTOMIZE_SHORTCUT_RESTORE_ALL:
    case NTP_CUSTOMIZE_SHORTCUT_TOGGLE_TYPE:
    case NTP_CUSTOMIZE_SHORTCUT_TOGGLE_VISIBILITY:
      UMA_HISTOGRAM_ENUMERATION("NewTabPage.CustomizeShortcutAction",
                                LoggingEventToCustomizeShortcutAction(event));
      break;
    case NTP_MIDDLE_SLOT_PROMO_SHOWN:
      UMA_HISTOGRAM_LOAD_TIME("NewTabPage.Promos.ShownTime", time);
      EmitNtpTraceEvent("NewTabPage.Promos.ShownTime", time);

      break;
    case NTP_MIDDLE_SLOT_PROMO_LINK_CLICKED:
      UMA_HISTOGRAM_EXACT_LINEAR("NewTabPage.Promos.LinkClicked", 1, 1);
      break;
    case NTP_BACKGROUND_UPLOAD_FROM_DEVICE:
    case NTP_BACKGROUND_OPEN_COLLECTION:
    case NTP_BACKGROUND_SELECT_IMAGE:
    case NTP_BACKGROUND_IMAGE_SET:
    case NTP_BACKGROUND_BACK_CLICK:
    case NTP_BACKGROUND_DEFAULT_SELECTED:
    case NTP_BACKGROUND_UPLOAD_CANCEL:
    case NTP_BACKGROUND_UPLOAD_DONE:
    case NTP_BACKGROUND_IMAGE_RESET:
    case NTP_BACKGROUND_REFRESH_TOGGLE_CLICKED:
    case NTP_BACKGROUND_DAILY_REFRESH_ENABLED:
      RecordAction(LoggingEventToBackgroundUserActionName(event));
      break;
    case NTP_CUSTOMIZATION_MENU_OPENED:
    case NTP_CUSTOMIZATION_MENU_CANCEL:
    case NTP_CUSTOMIZATION_MENU_DONE:
      RecordAction(LoggingEventToMenuUserActionName(event));
      break;
    case NTP_CUSTOMIZE_SHORTCUT_CUSTOM_LINKS_CLICKED:
    case NTP_CUSTOMIZE_SHORTCUT_MOST_VISITED_CLICKED:
    case NTP_CUSTOMIZE_SHORTCUT_VISIBILITY_TOGGLE_CLICKED:
      RecordAction(LoggingEventToShortcutUserActionName(event));
      break;
    case NTP_APP_RENDERED:
      UMA_HISTOGRAM_LOAD_TIME("NewTabPage.MainUi.ShownTime", time);
      EmitNtpTraceEvent("NewTabPage.MainUi.ShownTime", time);
      break;
  }
}

void NTPUserDataLogger::LogMostVisitedLoaded(base::TimeDelta time,
                                             bool using_most_visited,
                                             bool is_visible) {
  EmitNtpStatistics(time, using_most_visited, is_visible);
}

void NTPUserDataLogger::LogMostVisitedImpression(
    const ntp_tiles::NTPTileImpression& impression) {
  if ((impression.index >= ntp_tiles::kMaxNumTiles) ||
      logged_impressions_[impression.index].has_value()) {
    return;
  }
  logged_impressions_[impression.index] = impression;
}

void NTPUserDataLogger::LogMostVisitedNavigation(
    const ntp_tiles::NTPTileImpression& impression) {
  ntp_tiles::metrics::RecordTileClick(impression);

  // Records the action. This will be available as a time-stamped stream
  // server-side and can be used to compute time-to-long-dwell.
  base::RecordAction(base::UserMetricsAction("MostVisited_Clicked"));
}

bool NTPUserDataLogger::DefaultSearchProviderIsGoogle() const {
  return search::DefaultSearchProviderIsGoogle(profile_);
}

bool NTPUserDataLogger::CustomBackgroundIsConfigured() const {
  return NtpCustomBackgroundServiceFactory::GetForProfile(profile_)
      ->IsCustomBackgroundSet();
}

void NTPUserDataLogger::EmitNtpStatistics(base::TimeDelta load_time,
                                          bool using_most_visited,
                                          bool is_visible) {
  // We only send statistics once per page.
  if (has_emitted_) {
    return;
  }

  int tiles_count = 0;
  for (const absl::optional<ntp_tiles::NTPTileImpression>& impression :
       logged_impressions_) {
    if (!impression.has_value()) {
      break;
    }
    ntp_tiles::metrics::RecordTileImpression(*impression);
    ++tiles_count;
  }
  ntp_tiles::metrics::RecordPageImpression(tiles_count);

  DVLOG(1) << "Emitting NTP load time: " << load_time << ", "
           << "number of tiles: " << tiles_count;

  UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime", load_time);
  UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.MostVisited", load_time);

  // Note: This could be inaccurate if the default search engine was changed
  // since the page load started. That's unlikely enough to not warrant special
  // handling.
  bool is_google = DefaultSearchProviderIsGoogle();

  // Split between NTP variants.
  if (ntp_url_ == GURL(chrome::kChromeUINewTabPageURL)) {
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.WebUINTP", load_time);
  } else if (ntp_url_ == GURL(chrome::kChromeUINewTabPageThirdPartyURL)) {
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.WebUI3PNTP", load_time);
  }

  // Split between Startup and non-startup.
  if (during_startup_) {
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.Startup", load_time);
  } else {
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.NewTab", load_time);
  }

  if (is_google) {
    LogCustomizedShortcutSettings(using_most_visited, is_visible);

    if (!using_most_visited) {
      UMA_HISTOGRAM_ENUMERATION(
          "NewTabPage.Customized",
          LoggingEventToCustomizedFeature(NTP_SHORTCUT_CUSTOMIZED));
    }

    if (CustomBackgroundIsConfigured()) {
      UMA_HISTOGRAM_ENUMERATION(
          "NewTabPage.Customized",
          LoggingEventToCustomizedFeature(NTP_BACKGROUND_CUSTOMIZED));
    }
  }

  has_emitted_ = true;
  during_startup_ = false;
}

void NTPUserDataLogger::EmitNtpTraceEvent(const char* event_name,
                                          base::TimeDelta duration) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(kUIEventCategory, event_name,
                                                   TRACE_ID_LOCAL(this),
                                                   ntp_navigation_start_time_);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      kUIEventCategory, event_name, TRACE_ID_LOCAL(this),
      ntp_navigation_start_time_ + duration);
}

void NTPUserDataLogger::RecordDoodleImpression(base::TimeDelta time,
                                               bool is_cta,
                                               bool from_cache) {
  LogoImpressionType logo_type =
      is_cta ? LOGO_IMPRESSION_TYPE_CTA : LOGO_IMPRESSION_TYPE_STATIC;
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.LogoShown", logo_type,
                            LOGO_IMPRESSION_TYPE_MAX);
  EmitNtpTraceEvent("NewTabPage.LogoShown", time);

  if (from_cache) {
    UMA_HISTOGRAM_ENUMERATION("NewTabPage.LogoShown.FromCache", logo_type,
                              LOGO_IMPRESSION_TYPE_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION("NewTabPage.LogoShown.Fresh", logo_type,
                              LOGO_IMPRESSION_TYPE_MAX);
  }

  if (should_record_doodle_load_time_) {
    UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.LogoShownTime2", time);
    should_record_doodle_load_time_ = false;
  }
}

void NTPUserDataLogger::RecordAction(const char* action) {
  if (!action || !DefaultSearchProviderIsGoogle())
    return;

  base::RecordAction(base::UserMetricsAction(action));
}
