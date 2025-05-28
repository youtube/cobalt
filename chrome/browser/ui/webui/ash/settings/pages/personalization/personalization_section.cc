// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/personalization/personalization_section.h"

#include <array>
#include <optional>

#include "base/containers/span.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_features_util.h"
#include "chrome/browser/ui/webui/ash/settings/pages/personalization/personalization_hub_handler.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kPersonalizationSectionPath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
}  // namespace mojom

namespace {

base::span<const SearchConcept> GetPersonalizationSearchConcepts() {
  static constexpr auto tags = std::to_array<SearchConcept>({
      {IDS_OS_SETTINGS_TAG_WALLPAPER_AND_STYLE,
       mojom::kPersonalizationSectionPath,
       mojom::SearchResultIcon::kPersonalization,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kPersonalization}},
  });
  return tags;
}

}  // namespace

PersonalizationSection::PersonalizationSection(
    Profile* profile,
    SearchTagRegistry* search_tag_registry,
    PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetPersonalizationSearchConcepts());
}

PersonalizationSection::~PersonalizationSection() = default;

void PersonalizationSection::AddLoadTimeData(
    content::WebUIDataSource* html_source) {
  auto* user = BrowserContextHelper::Get()->GetUserByBrowserContext(profile());
  const bool kIsGuest = IsGuestModeActive(user);

  webui::LocalizedString kWallpaperLocalizedStrings[] = {
      {"personalizationPageTitle", IDS_OS_SETTINGS_PERSONALIZATION},
      {"personalizationMenuItemDescription",
       kIsGuest
           ? IDS_OS_SETTINGS_PERSONALIZATION_MENU_ITEM_DESCRIPTION_GUEST_MODE
           : IDS_OS_SETTINGS_PERSONALIZATION_MENU_ITEM_DESCRIPTION},
      {"personalizationHubTitle", IDS_OS_SETTINGS_OPEN_PERSONALIZATION_HUB},
      {"personalizationHubSubtitle",
       kIsGuest ? IDS_OS_SETTINGS_OPEN_PERSONALIZATION_HUB_SUBTITLE_GUEST_MODE
                : IDS_OS_SETTINGS_OPEN_PERSONALIZATION_HUB_SUBTITLE},
  };

  html_source->AddLocalizedStrings(kWallpaperLocalizedStrings);
}

void PersonalizationSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(std::make_unique<PersonalizationHubHandler>());
}

int PersonalizationSection::GetSectionNameMessageId() const {
  return IDS_OS_SETTINGS_PERSONALIZATION;
}

mojom::Section PersonalizationSection::GetSection() const {
  return mojom::Section::kPersonalization;
}

mojom::SearchResultIcon PersonalizationSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kPaintbrush;
}

const char* PersonalizationSection::GetSectionPath() const {
  return mojom::kPersonalizationSectionPath;
}

bool PersonalizationSection::LogMetric(mojom::Setting setting,
                                       base::Value& value) const {
  // Unimplemented.
  return false;
}

void PersonalizationSection::RegisterHierarchy(
    HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kOpenWallpaper);
}

}  // namespace ash::settings
