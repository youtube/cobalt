// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/date_time_section.h"

#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/settings/ash/date_time_handler.h"
#include "chrome/browser/ui/webui/settings/ash/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/system_settings_provider.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kDateAndTimeSectionPath;
using ::chromeos::settings::mojom::kTimeZoneSubpagePath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

const std::vector<SearchConcept>& GetDateTimeSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_DATE_TIME,
       mojom::kDateAndTimeSectionPath,
       mojom::SearchResultIcon::kClock,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kDateAndTime}},
      {IDS_OS_SETTINGS_TAG_DATE_TIME_MILITARY_CLOCK,
       mojom::kDateAndTimeSectionPath,
       mojom::SearchResultIcon::kClock,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::k24HourClock},
       {IDS_OS_SETTINGS_TAG_DATE_TIME_MILITARY_CLOCK_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetFineGrainedTimeZoneSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_DATE_TIME_ZONE,
       mojom::kTimeZoneSubpagePath,
       mojom::SearchResultIcon::kClock,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kChangeTimeZone}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetNoFineGrainedTimeZoneSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_DATE_TIME_ZONE,
       mojom::kDateAndTimeSectionPath,
       mojom::SearchResultIcon::kClock,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kChangeTimeZone}},
  });
  return *tags;
}

bool IsFineGrainedTimeZoneEnabled() {
  SystemSettingsProvider provider;
  return provider.Get(kFineGrainedTimeZoneResolveEnabled)->GetBool();
}

}  // namespace

DateTimeSection::DateTimeSection(Profile* profile,
                                 SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetDateTimeSearchConcepts());

  if (IsFineGrainedTimeZoneEnabled())
    updater.AddSearchTags(GetFineGrainedTimeZoneSearchConcepts());
  else
    updater.AddSearchTags(GetNoFineGrainedTimeZoneSearchConcepts());
}

DateTimeSection::~DateTimeSection() = default;

void DateTimeSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"dateTimePageTitle", IDS_SETTINGS_DATE_TIME},
      {"timeZone", IDS_SETTINGS_TIME_ZONE},
      {"selectTimeZoneResolveMethod",
       IDS_SETTINGS_SELECT_TIME_ZONE_RESOLVE_METHOD},
      {"timeZoneGeolocation", IDS_SETTINGS_TIME_ZONE_GEOLOCATION},
      {"timeZoneButton", IDS_SETTINGS_TIME_ZONE_BUTTON},
      {"timeZoneSubpageTitle", IDS_SETTINGS_TIME_ZONE_SUBPAGE_TITLE},
      {"setTimeZoneAutomaticallyDisabled",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_DISABLED},
      {"setTimeZoneAutomaticallyOn",
       IDS_SETTINGS_TIME_ZONE_DETECTION_SET_AUTOMATICALLY},
      {"setTimeZoneAutomaticallyOff",
       IDS_SETTINGS_TIME_ZONE_DETECTION_CHOOSE_FROM_LIST},
      {"setTimeZoneAutomaticallyIpOnlyDefault",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_IP_ONLY_DEFAULT},
      {"setTimeZoneAutomaticallyWithWiFiAccessPointsData",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_SEND_WIFI_AP},
      {"setTimeZoneAutomaticallyWithAllLocationInfo",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_SEND_ALL_INFO},
      {"use24HourClock", IDS_SETTINGS_USE_24_HOUR_CLOCK},
      {"setDateTime", IDS_SETTINGS_SET_DATE_TIME},
      {"timeZoneGeolocationDialogTitle",
       IDS_SETTINGS_TIME_ZONE_GEOLOCATION_DIALOG_TITLE},
      {"timeZoneGeolocationDialogBody",
       IDS_SETTINGS_TIME_ZONE_GEOLOCATION_DIALOG_BODY},
      {"timeZoneGeolocationDialogCancelButton",
       IDS_SETTINGS_TIME_ZONE_GEOLOCATION_DIALOG_CANCEL_BUTTON},
      {"timeZoneGeolocationDialogTurnOnButton", IDS_SETTINGS_TURN_ON}};
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString(
      "timeZoneGeolocationDialogLearnMoreUrl",
      base::ASCIIToUTF16(base::StringPrintf(
          chrome::kTimeZoneSettingsLearnMoreURL,
          g_browser_process->GetApplicationLocale().c_str())));

  html_source->AddString(
      "timeZoneSettingsLearnMoreURL",
      base::ASCIIToUTF16(base::StringPrintf(
          chrome::kTimeZoneSettingsLearnMoreURL,
          g_browser_process->GetApplicationLocale().c_str())));

  // Set the initial time zone to show.
  html_source->AddString("timeZoneName", system::GetCurrentTimezoneName());
  html_source->AddString(
      "timeZoneID",
      system::TimezoneSettings::GetInstance()->GetCurrentTimezoneID());

  bool is_child = user_manager::UserManager::Get()->GetActiveUser()->IsChild();
  html_source->AddBoolean("isChild", is_child);
}

void DateTimeSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(std::make_unique<DateTimeHandler>());
}

int DateTimeSection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_DATE_TIME;
}

mojom::Section DateTimeSection::GetSection() const {
  return mojom::Section::kDateAndTime;
}

mojom::SearchResultIcon DateTimeSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kClock;
}

std::string DateTimeSection::GetSectionPath() const {
  return mojom::kDateAndTimeSectionPath;
}

bool DateTimeSection::LogMetric(mojom::Setting setting,
                                base::Value& value) const {
  // Unimplemented.
  return false;
}

void DateTimeSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::k24HourClock);

  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_TIME_ZONE_SUBPAGE_TITLE, mojom::Subpage::kTimeZone,
      mojom::SearchResultIcon::kClock, mojom::SearchResultDefaultRank::kMedium,
      mojom::kTimeZoneSubpagePath);

  // When fine-grained time zone is enabled, users change the time zone on the
  // time zone subpage; otherwise, the setting is directly embedded in the
  // section.
  if (IsFineGrainedTimeZoneEnabled()) {
    generator->RegisterNestedSetting(mojom::Setting::kChangeTimeZone,
                                     mojom::Subpage::kTimeZone);
  } else {
    generator->RegisterTopLevelSetting(mojom::Setting::kChangeTimeZone);
  }
}

}  // namespace ash::settings
