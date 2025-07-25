// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_prefs.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_sync_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/tab_search_resources.h"
#include "chrome/grit/tab_search_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/style/platform_style.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

TabSearchUI::TabSearchUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui,
                                    true /* Needed for webui browser tests */),
      webui_load_timer_(web_ui->GetWebContents(),
                        "Tabs.TabSearch.WebUI.LoadDocumentTime",
                        "Tabs.TabSearch.WebUI.LoadCompletedTime") {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUITabSearchHost);
  static constexpr webui::LocalizedString kStrings[] = {
      // Tab search UI strings
      {"a11yTabClosed", IDS_TAB_SEARCH_A11Y_TAB_CLOSED},
      {"a11yFoundTab", IDS_TAB_SEARCH_A11Y_FOUND_TAB},
      {"a11yFoundTabFor", IDS_TAB_SEARCH_A11Y_FOUND_TAB_FOR},
      {"a11yFoundTabs", IDS_TAB_SEARCH_A11Y_FOUND_TABS},
      {"a11yFoundTabsFor", IDS_TAB_SEARCH_A11Y_FOUND_TABS_FOR},
      {"a11yOpenTab", IDS_TAB_SEARCH_A11Y_OPEN_TAB},
      {"a11yRecentlyClosedTab", IDS_TAB_SEARCH_A11Y_RECENTLY_CLOSED_TAB},
      {"a11yRecentlyClosedTabGroup",
       IDS_TAB_SEARCH_A11Y_RECENTLY_CLOSED_TAB_GROUP},
      {"audioMuting", IDS_TAB_AX_LABEL_AUDIO_MUTING_FORMAT},
      {"audioPlaying", IDS_TAB_AX_LABEL_AUDIO_PLAYING_FORMAT},
      {"clearSearch", IDS_CLEAR_SEARCH},
      {"closeTab", IDS_TAB_SEARCH_CLOSE_TAB},
      {"collapseRecentlyClosed", IDS_TAB_SEARCH_COLLAPSE_RECENTLY_CLOSED},
      {"expandRecentlyClosed", IDS_TAB_SEARCH_EXPAND_RECENTLY_CLOSED},
      {"mediaRecording", IDS_TAB_AX_LABEL_MEDIA_RECORDING_FORMAT},
      {"mediaTabs", IDS_TAB_SEARCH_MEDIA_TABS},
      {"noResultsFound", IDS_TAB_SEARCH_NO_RESULTS_FOUND},
      {"openTabs", IDS_TAB_SEARCH_OPEN_TABS},
      {"oneTab", IDS_TAB_SEARCH_ONE_TAB},
      {"recentlyClosed", IDS_TAB_SEARCH_RECENTLY_CLOSED},
      {"recentlyClosedExpandA11yLabel",
       IDS_TAB_SEARCH_EXPAND_RECENTLY_CLOSED_ITEMS},
      {"searchTabs", IDS_TAB_SEARCH_SEARCH_TABS},
      {"tabCount", IDS_TAB_SEARCH_TAB_COUNT},
      {"tabSearchTabName", IDS_TAB_SEARCH_TAB_NAME},
      // Tab organization UI strings
      {"createGroup", IDS_TAB_ORGANIZATION_CREATE_GROUP},
      {"dismiss", IDS_TAB_ORGANIZATION_DISMISS},
      {"failureBodyGeneric", IDS_TAB_ORGANIZATION_FAILURE_BODY_GENERIC},
      {"failureBodyGrouping", IDS_TAB_ORGANIZATION_FAILURE_BODY_GROUPING},
      {"failureTitleGeneric", IDS_TAB_ORGANIZATION_FAILURE_TITLE_GENERIC},
      {"failureTitleGrouping", IDS_TAB_ORGANIZATION_FAILURE_TITLE_GROUPING},
      {"inProgressTitle", IDS_TAB_ORGANIZATION_IN_PROGRESS_TITLE},
      {"notStartedBody", IDS_TAB_ORGANIZATION_NOT_STARTED_BODY},
      {"notStartedBodyFRE", IDS_TAB_ORGANIZATION_NOT_STARTED_BODY_FRE},
      {"notStartedBodyUnsynced",
       IDS_TAB_ORGANIZATION_NOT_STARTED_BODY_UNSYNCED},
      {"notStartedBodyUnsyncedHistory",
       IDS_TAB_ORGANIZATION_NOT_STARTED_BODY_UNSYNCED_HISTORY},
      {"notStartedButton", IDS_TAB_ORGANIZATION_NOT_STARTED_BUTTON},
      {"notStartedButtonUnsynced",
       IDS_TAB_ORGANIZATION_NOT_STARTED_BUTTON_UNSYNCED},
      {"notStartedButtonUnsyncedHistory",
       IDS_TAB_ORGANIZATION_NOT_STARTED_BUTTON_UNSYNCED_HISTORY},
      {"notStartedButtonSyncPaused",
       IDS_TAB_ORGANIZATION_NOT_STARTED_BUTTON_SYNC_PAUSED},
      {"notStartedTitle", IDS_TAB_ORGANIZATION_NOT_STARTED_TITLE},
      {"notStartedTitleFRE", IDS_TAB_ORGANIZATION_NOT_STARTED_TITLE_FRE},
      {"successTitle", IDS_TAB_ORGANIZATION_SUCCESS_TITLE},
      {"tabOrganizationTabName", IDS_TAB_ORGANIZATION_TAB_NAME},
      {"tipTitle", IDS_TAB_ORGANIZATION_TIP_TITLE},
      {"tipBody", IDS_TAB_ORGANIZATION_TIP_BODY},
      {"tipAction", IDS_TAB_ORGANIZATION_TIP_ACTION},
  };
  webui::SetupChromeRefresh2023(source);
  source->AddLocalizedStrings(kStrings);
  source->AddBoolean("useRipples", views::PlatformStyle::kUseRipples);

  // Add the configuration parameters for fuzzy search.
  source->AddBoolean("useFuzzySearch", base::FeatureList::IsEnabled(
                                           features::kTabSearchFuzzySearch));

  source->AddBoolean(
      "useMetricsReporter",
      base::FeatureList::IsEnabled(features::kTabSearchUseMetricsReporter));

  source->AddBoolean("searchIgnoreLocation",
                     features::kTabSearchSearchIgnoreLocation.Get());
  source->AddInteger("searchDistance",
                     features::kTabSearchSearchDistance.Get());
  source->AddDouble(
      "searchThreshold",
      std::clamp<double>(features::kTabSearchSearchThreshold.Get(),
                          features::kTabSearchSearchThresholdMin,
                          features::kTabSearchSearchThresholdMax));
  source->AddDouble("searchTitleWeight", features::kTabSearchTitleWeight.Get());
  source->AddDouble("searchHostnameWeight",
                    features::kTabSearchHostnameWeight.Get());
  source->AddDouble("searchGroupTitleWeight",
                    features::kTabSearchGroupTitleWeight.Get());

  source->AddBoolean("moveActiveTabToBottom",
                     features::kTabSearchMoveActiveTabToBottom.Get());
  source->AddLocalizedString("close", IDS_CLOSE);

  source->AddInteger(
      "recentlyClosedDefaultItemDisplayCount",
      features::kTabSearchRecentlyClosedDefaultItemDisplayCount.Get());

  source->AddBoolean("tabOrganizationEnabled", features::IsTabOrganization());
  source->AddInteger("tabIndex", TabIndex());
  source->AddBoolean("showTabOrganizationFRE", ShowTabOrganizationFRE());

  ui::Accelerator accelerator(ui::VKEY_A,
                              ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR);
  source->AddString("shortcutText", accelerator.GetShortcutText());

  webui::SetupWebUIDataSource(
      source, base::make_span(kTabSearchResources, kTabSearchResourcesSize),
      IDR_TAB_SEARCH_TAB_SEARCH_HTML);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  web_ui->AddMessageHandler(std::make_unique<TabSearchSyncHandler>(profile));

  page_handler_timer_ = base::ElapsedTimer();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "browser", "TabSearchPageHandlerConstructionDelay", this);
}

TabSearchUI::~TabSearchUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(TabSearchUI)

void TabSearchUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

void TabSearchUI::BindInterface(
    mojo::PendingReceiver<tab_search::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void TabSearchUI::BindInterface(
    mojo::PendingReceiver<metrics_reporter::mojom::PageMetricsHost> receiver) {
  metrics_reporter_.BindInterface(std::move(receiver));
}

void TabSearchUI::CreatePageHandler(
    mojo::PendingRemote<tab_search::mojom::Page> page,
    mojo::PendingReceiver<tab_search::mojom::PageHandler> receiver) {
  DCHECK(page);

  // CreatePageHandler() can be called multiple times if reusing the same
  // WebUIController. For eg refreshing the page will create new PageHandler but
  // reuse TabSearchUI. Check to make sure |page_handler_timer_| is valid before
  // logging metrics.
  if (page_handler_timer_.has_value()) {
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "browser", "TabSearchPageHandlerConstructionDelay", this);
    UmaHistogramMediumTimes("Tabs.TabSearch.PageHandlerConstructionDelay",
                            page_handler_timer_->Elapsed());
    page_handler_timer_.reset();
  }

  // TODO(tluk): Investigate whether we can avoid recreating this multiple times
  // per instance of the TabSearchUI.
  page_handler_ = std::make_unique<TabSearchPageHandler>(
      std::move(receiver), std::move(page), web_ui(), this, &metrics_reporter_);
}

bool TabSearchUI::ShowTabOrganizationFRE() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  return prefs->GetBoolean(tab_search_prefs::kTabOrganizationShowFRE);
}

int TabSearchUI::TabIndex() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  return prefs->GetInteger(tab_search_prefs::kTabSearchTabIndex);
}
