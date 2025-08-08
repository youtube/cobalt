// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/bookmarks/bookmarks_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/feed/feed_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/performance_controls/performance_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/views/side_panel/reading_list/reading_list_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/user_note/user_note_ui_coordinator.h"
#include "components/feed/feed_feature_list.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/performance_manager/public/features.h"
#include "components/prefs/pref_service.h"
#include "components/user_notes/user_notes_features.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"
#include "extensions/common/extension_features.h"
#endif

// static
void SidePanelUtil::PopulateGlobalEntries(Browser* browser,
                                          SidePanelRegistry* global_registry) {
  // Add reading list.
  ReadingListSidePanelCoordinator::GetOrCreateForBrowser(browser)
      ->CreateAndRegisterEntry(global_registry);

  // Add bookmarks.
  BookmarksSidePanelCoordinator::GetOrCreateForBrowser(browser)
      ->CreateAndRegisterEntry(global_registry);

  // Add performance.
  if (base::FeatureList::IsEnabled(
          performance_manager::features::kPerformanceControlsSidePanel)) {
    PerformanceSidePanelCoordinator::GetOrCreateForBrowser(browser)
        ->CreateAndRegisterEntry(global_registry);
  }

  // Add history clusters.
  if (HistoryClustersSidePanelCoordinator::IsSupported(browser->profile())) {
    HistoryClustersSidePanelCoordinator::GetOrCreateForBrowser(browser)
        ->CreateAndRegisterEntry(global_registry);
  }

  // Add read anything.
  if (features::IsReadAnythingEnabled()) {
    ReadAnythingCoordinator::GetOrCreateForBrowser(browser)
        ->CreateAndRegisterEntry(global_registry);
  }

  // Create Search Companion coordinator.
  // Disable runtime checks so that coordinator can monitor the runtime changes
  // in the availability of companion.
  if (companion::IsCompanionFeatureEnabled() &&
      SearchCompanionSidePanelCoordinator::IsSupported(
          browser->profile(),
          /*include_runtime_checks=*/false)) {
    SearchCompanionSidePanelCoordinator::GetOrCreateForBrowser(browser);
  }

  // Add user notes.
  if (user_notes::IsUserNotesEnabled()) {
    UserNoteUICoordinator::GetOrCreateForBrowser(browser)
        ->CreateAndRegisterEntry(global_registry);
  }

  // Add feed.
  if (base::FeatureList::IsEnabled(feed::kWebUiFeed)) {
    feed::FeedSidePanelCoordinator::GetOrCreateForBrowser(browser)
        ->CreateAndRegisterEntry(global_registry);
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionSidePanelIntegration)) {
    extensions::ExtensionSidePanelManager::GetOrCreateForBrowser(browser);
  }
#endif

  return;
}

SidePanelContentProxy* SidePanelUtil::GetSidePanelContentProxy(
    views::View* content_view) {
  if (!content_view->GetProperty(kSidePanelContentProxyKey))
    content_view->SetProperty(
        kSidePanelContentProxyKey,
        std::make_unique<SidePanelContentProxy>(true).release());
  return content_view->GetProperty(kSidePanelContentProxyKey);
}

std::unique_ptr<views::View> SidePanelUtil::DeregisterAndReturnView(
    SidePanelRegistry* registry,
    SidePanelEntry::Key key) {
  std::unique_ptr<SidePanelEntry> entry =
      registry->DeregisterAndReturnEntry(key);
  return entry->CachedView() ? entry->GetContent() : nullptr;
}

SidePanelCoordinator* SidePanelUtil::GetSidePanelCoordinatorForBrowser(
    Browser* browser) {
  return static_cast<SidePanelCoordinator*>(
      SidePanelUI::GetSidePanelUIForBrowser(browser));
}

void SidePanelUtil::RecordSidePanelOpen(
    absl::optional<SidePanelUtil::SidePanelOpenTrigger> trigger) {
  base::RecordAction(base::UserMetricsAction("SidePanel.Show"));

  if (trigger.has_value())
    base::UmaHistogramEnumeration("SidePanel.OpenTrigger", trigger.value());
}

void SidePanelUtil::RecordSidePanelShowOrChangeEntryTrigger(
    absl::optional<SidePanelUtil::SidePanelOpenTrigger> trigger) {
  if (trigger.has_value()) {
    base::UmaHistogramEnumeration("SidePanel.OpenOrChangeEntryTrigger",
                                  trigger.value());
  }
}

void SidePanelUtil::RecordSidePanelClosed(base::TimeTicks opened_timestamp) {
  base::RecordAction(base::UserMetricsAction("SidePanel.Hide"));

  base::UmaHistogramLongTimes("SidePanel.OpenDuration",
                              base::TimeTicks::Now() - opened_timestamp);
}

void SidePanelUtil::RecordSidePanelResizeMetrics(SidePanelEntry::Id id,
                                                 int side_panel_contents_width,
                                                 int browser_window_width) {
  std::string entry_name = SidePanelEntryIdToHistogramName(id);

  // Metrics per-id and overall for side panel width after resize.
  base::UmaHistogramCounts10000(
      base::StrCat({"SidePanel.", entry_name, ".ResizedWidth"}),
      side_panel_contents_width);
  base::UmaHistogramCounts10000("SidePanel.ResizedWidth",
                                side_panel_contents_width);

  // Metrics per-id and overall for side panel width after resize as a
  // percentage of browser width.
  int width_percentage = side_panel_contents_width * 100 / browser_window_width;
  base::UmaHistogramPercentage(
      base::StrCat({"SidePanel.", entry_name, ".ResizedWidthPercentage"}),
      width_percentage);
  base::UmaHistogramPercentage("SidePanel.ResizedWidthPercentage",
                               width_percentage);
}

void SidePanelUtil::RecordNewTabButtonClicked(SidePanelEntry::Id id) {
  base::RecordComputedAction(
      base::StrCat({"SidePanel.", SidePanelEntryIdToHistogramName(id),
                    ".NewTabButtonClicked"}));
}

void SidePanelUtil::RecordEntryShownMetrics(
    SidePanelEntry::Id id,
    base::TimeTicks load_started_timestamp) {
  base::RecordComputedAction(base::StrCat(
      {"SidePanel.", SidePanelEntryIdToHistogramName(id), ".Shown"}));
  if (load_started_timestamp != base::TimeTicks()) {
    base::UmaHistogramLongTimes(
        base::StrCat({"SidePanel.", SidePanelEntryIdToHistogramName(id),
                      ".TimeFromEntryTriggerToShown"}),
        base::TimeTicks::Now() - load_started_timestamp);
  }
}

void SidePanelUtil::RecordEntryHiddenMetrics(SidePanelEntry::Id id,
                                             base::TimeTicks shown_timestamp) {
  base::UmaHistogramLongTimes(
      base::StrCat({"SidePanel.", SidePanelEntryIdToHistogramName(id),
                    ".ShownDuration"}),
      base::TimeTicks::Now() - shown_timestamp);
}

void SidePanelUtil::RecordEntryShowTriggeredMetrics(
    Browser* browser,
    SidePanelEntry::Id id,
    absl::optional<SidePanelUtil::SidePanelOpenTrigger> trigger) {
  if (trigger.has_value()) {
    base::UmaHistogramEnumeration(
        base::StrCat({"SidePanel.", SidePanelEntryIdToHistogramName(id),
                      ".ShowTriggered"}),
        trigger.value());
  }

  if (id == SidePanelEntry::Id::kSearchCompanion) {
    auto* search_companion_coordinator =
        SearchCompanionSidePanelCoordinator::GetOrCreateForBrowser(browser);
    search_companion_coordinator->NotifyCompanionOfSidePanelOpenTrigger(
        trigger);
  }
}

void SidePanelUtil::RecordComboboxShown() {
  base::UmaHistogramBoolean("SidePanel.ComboboxMenuShown", true);
}
