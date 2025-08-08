// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/bookmarks/url_and_id.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/common/channel_info.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_sync_bridge.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/web_contents.h"

namespace {
constexpr base::TimeDelta kDelayBeforeMetricsLogged = base::Hours(1);

std::unique_ptr<syncer::ClientTagBasedModelTypeProcessor>
CreateChangeProcessor() {
  return std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
      syncer::SAVED_TAB_GROUP,
      base::BindRepeating(&syncer::ReportUnrecoverableError,
                          chrome::GetChannel()));
}

}  // anonymous namespace

SavedTabGroupKeyedService::SavedTabGroupKeyedService(Profile* profile)
    : profile_(profile),
      listener_(model(), profile),
      bridge_(model(), GetStoreFactory(), CreateChangeProcessor()) {
  model()->AddObserver(this);

  metrics_timer_.Start(
      FROM_HERE, kDelayBeforeMetricsLogged,
      base::BindRepeating(&SavedTabGroupKeyedService::RecordMetrics,
                          base::Unretained(this)));
}

SavedTabGroupKeyedService::~SavedTabGroupKeyedService() {
  model_.RemoveObserver(this);
}

syncer::OnceModelTypeStoreFactory SavedTabGroupKeyedService::GetStoreFactory() {
  DCHECK(ModelTypeStoreServiceFactory::GetForProfile(profile()));
  return ModelTypeStoreServiceFactory::GetForProfile(profile())
      ->GetStoreFactory();
}

void SavedTabGroupKeyedService::StoreLocalToSavedId(
    const base::Uuid& saved_guid,
    const tab_groups::TabGroupId local_group_id) {
  // Avoid linking SavedTabGroups that are already open.
  const SavedTabGroup* const group = model()->Get(saved_guid);
  if (group && group->local_group_id().has_value()) {
    return;
  }

  // If there is no saved group with guid `saved_guid`, the group must
  // have been unsaved since this session closed.
  if (model()->is_loaded() && !group) {
    return;
  }

  // The model could already be loaded when restoring groups from a previously
  // crashed session / window. This means we will have to manually trigger the
  // local to saved group linking.
  if (model()->is_loaded()) {
    ConnectLocalTabGroup(local_group_id, saved_guid);
  } else {
    saved_guid_to_local_group_id_mapping_.emplace_back(saved_guid,
                                                       local_group_id);
  }
}

void SavedTabGroupKeyedService::OpenSavedTabGroupInBrowser(
    Browser* browser,
    const base::Uuid& saved_group_guid) {
  const SavedTabGroup* saved_group = model_.Get(saved_group_guid);

  // In the case where this function is called after confirmation of an
  // interstitial, the saved_group could be null, so protect against this by
  // early returning.
  if (!saved_group) {
    return;
  }

  // Activate the first tab in a group if it is already open.
  if (saved_group->local_group_id().has_value()) {
    FocusFirstTabOrWindowInOpenGroup(saved_group->local_group_id().value());
    return;
  }

  // If our tab group was not found in any tabstrip model, open the group in
  // this browser's tabstrip model.
  TabStripModel* tab_strip_model_for_creation = browser->tab_strip_model();
  std::map<content::WebContents*, base::Uuid> opened_web_contents_to_uuid =
      GetWebContentsToTabGuidMappingForOpening(browser, saved_group,
                                               saved_group_guid);

  // If no tabs were opened, then there's nothing to do.
  if (opened_web_contents_to_uuid.empty()) {
    return;
  }

  // Figure out which tabs we actually opened in this browser that aren't
  // already in groups.
  std::vector<int> tab_indices;
  for (int i = 0; i < tab_strip_model_for_creation->count(); ++i) {
    if (base::Contains(opened_web_contents_to_uuid,
                       tab_strip_model_for_creation->GetWebContentsAt(i)) &&
        !tab_strip_model_for_creation->GetTabGroupForTab(i).has_value()) {
      tab_indices.push_back(i);
    }
  }

  // Create a new group in the tabstrip.
  tab_groups::TabGroupId tab_group_id = tab_groups::TabGroupId::GenerateNew();
  tab_strip_model_for_creation->AddToGroupForRestore(tab_indices, tab_group_id);

  // Update the saved tab group to link to the local group id.
  model_.OnGroupOpenedInTabStrip(saved_group->saved_guid(), tab_group_id);

  TabGroup* const tab_group =
      tab_strip_model_for_creation->group_model()->GetTabGroup(tab_group_id);

  // Activate the first tab in the tab group.
  absl::optional<int> first_tab = tab_group->GetFirstTab();
  DCHECK(first_tab.has_value());
  tab_strip_model_for_creation->ActivateTabAt(first_tab.value());

  // Set the group's visual data after the tab strip is in its final state. This
  // ensures the tab group's bounds are correctly set. crbug/1408814.
  UpdateGroupVisualData(saved_group_guid,
                        saved_group->local_group_id().value());

  listener_.ConnectToLocalTabGroup(*model_.Get(saved_group_guid),
                                   opened_web_contents_to_uuid);

  base::RecordAction(
      base::UserMetricsAction("TabGroups_SavedTabGroups_Opened"));
}

void SavedTabGroupKeyedService::SaveGroup(
    const tab_groups::TabGroupId& group_id) {
  Browser* browser = SavedTabGroupUtils::GetBrowserWithTabGroupId(group_id);
  CHECK(browser);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  CHECK(tab_strip_model);
  CHECK(tab_strip_model->group_model());

  TabGroup* tab_group = tab_strip_model->group_model()->GetTabGroup(group_id);
  CHECK(tab_group);

  SavedTabGroup saved_tab_group(tab_group->visual_data()->title(),
                                tab_group->visual_data()->color(), {},
                                absl::nullopt, absl::nullopt, tab_group->id());

  // Build the SavedTabGroupTabs and add them to the SavedTabGroup.
  const gfx::Range tab_range = tab_group->ListTabs();

  std::map<content::WebContents*, base::Uuid> opened_web_contents_to_uuid;
  for (auto i = tab_range.start(); i < tab_range.end(); ++i) {
    content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
    CHECK(web_contents);

    SavedTabGroupTab saved_tab_group_tab =
        SavedTabGroupUtils::CreateSavedTabGroupTabFromWebContents(
            web_contents, saved_tab_group.saved_guid());

    opened_web_contents_to_uuid.emplace(web_contents,
                                        saved_tab_group_tab.saved_tab_guid());

    saved_tab_group.AddTabLocally(std::move(saved_tab_group_tab));
  }

  const base::Uuid saved_group_guid = saved_tab_group.saved_guid();
  model_.Add(std::move(saved_tab_group));

  // Link the local group to the saved group in the listener.
  listener_.ConnectToLocalTabGroup(*model_.Get(saved_group_guid),
                                   opened_web_contents_to_uuid);
}

void SavedTabGroupKeyedService::UnsaveGroup(
    const tab_groups::TabGroupId& group_id) {
  // Get the guid since disconnect removes the local id.
  const SavedTabGroup* group = model_.Get(group_id);
  CHECK(group);

  // Stop listening to the local group.
  DisconnectLocalTabGroup(group_id);

  // Unsave the group.
  model_.Remove(group->saved_guid());
}

void SavedTabGroupKeyedService::PauseTrackingLocalTabGroup(
    const tab_groups::TabGroupId& group_id) {
  listener_.PauseTrackingLocalTabGroup(group_id);
}

void SavedTabGroupKeyedService::ResumeTrackingLocalTabGroup(
    const base::Uuid& saved_group_guid,
    const tab_groups::TabGroupId& group_id) {
  listener_.ResumeTrackingLocalTabGroup(group_id);
}

void SavedTabGroupKeyedService::DisconnectLocalTabGroup(
    const tab_groups::TabGroupId& group_id) {
  listener_.DisconnectLocalTabGroup(group_id);

  // Stop listening to the current tab group and notify observers.
  model_.OnGroupClosedInTabStrip(group_id);
}

void SavedTabGroupKeyedService::ConnectLocalTabGroup(
    const tab_groups::TabGroupId& local_group_id,
    const base::Uuid& saved_guid) {
  Browser* const browser =
      SavedTabGroupUtils::GetBrowserWithTabGroupId(local_group_id);
  CHECK(browser);

  TabStripModel* const tab_strip_model = browser->tab_strip_model();
  CHECK(tab_strip_model);

  TabGroup* const tab_group =
      tab_strip_model->group_model()->GetTabGroup(local_group_id);
  CHECK(tab_group);

  const SavedTabGroup* const saved_group = model_.Get(saved_guid);
  CHECK(saved_group);

  const size_t tabs_in_group = tab_group->tab_count();
  const size_t tabs_in_saved_group = saved_group->saved_tabs().size();
  const bool saved_group_has_less_tabs = tabs_in_saved_group < tabs_in_group;
  const bool saved_group_has_more_tabs = tabs_in_saved_group > tabs_in_group;

  if (saved_group_has_more_tabs) {
    AddMissingTabsToOutOfSyncLocalTabGroup(browser, tab_group, saved_group);
  } else if (saved_group_has_less_tabs) {
    RemoveExtraTabsFromOutOfSyncLocalTabGroup(tab_strip_model, tab_group,
                                              saved_group);
  }

  const gfx::Range& tab_range = tab_group->ListTabs();
  CHECK(tab_range.length() == tabs_in_saved_group);

  UpdateWebContentsToMatchSavedTabGroupTabs(tab_strip_model, saved_group,
                                            tab_range);

  model_.OnGroupOpenedInTabStrip(saved_guid, local_group_id);
  UpdateGroupVisualData(saved_guid, local_group_id);

  listener_.ConnectToLocalTabGroup(
      *model_.Get(saved_guid), GetWebContentsToTabGuidMappingForSavedGroup(
                                   tab_strip_model, saved_group, tab_range));
}

void SavedTabGroupKeyedService::SavedTabGroupModelLoaded() {
  for (const auto& [saved_guid, local_group_id] :
       saved_guid_to_local_group_id_mapping_) {
    if (model()->is_loaded() && !model()->Contains(saved_guid)) {
      continue;
    }

    ConnectLocalTabGroup(local_group_id, saved_guid);
  }

  // Clear `saved_guid_to_local_group_id_mapping_` to save space when finished.
  //
  // TODO(dljames): Investigate using a single use callback to connect local and
  // saved groups together. There are crashes that occur when restarting the
  // browser before the browser process completely shuts down. The callback will
  // also remove the need of `saved_guid_to_local_group_id_mapping_`.
  saved_guid_to_local_group_id_mapping_.clear();
  CHECK(saved_guid_to_local_group_id_mapping_.empty());
}

void SavedTabGroupKeyedService::SavedTabGroupRemovedFromSync(
    const SavedTabGroup* removed_group) {
  // Do nothing if `removed_group` is not open in the tabstrip.
  if (!removed_group->local_group_id().has_value()) {
    return;
  }

  // Update the local group's contents to match the saved group's.
  listener_.RemoveLocalGroupFromSync(removed_group->local_group_id().value());
}

void SavedTabGroupKeyedService::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_guid,
    const absl::optional<base::Uuid>& tab_guid) {
  const SavedTabGroup* const saved_group = model_.Get(group_guid);
  CHECK(saved_group);

  // Do nothing if the saved group is not open in the tabstrip.
  if (!saved_group->local_group_id().has_value()) {
    return;
  }

  // Update the local group's contents to match the saved group's.
  listener_.UpdateLocalGroupFromSync(saved_group->local_group_id().value());
}

void SavedTabGroupKeyedService::AddMissingTabsToOutOfSyncLocalTabGroup(
    Browser* browser,
    const TabGroup* const tab_group,
    const SavedTabGroup* const saved_group) {
  size_t num_tabs_in_saved_group = saved_group->saved_tabs().size();
  size_t num_tabs_in_local_group = tab_group->tab_count();

  for (size_t relative_index_in_group = num_tabs_in_local_group;
       relative_index_in_group < num_tabs_in_saved_group;
       ++relative_index_in_group) {
    const GURL url_to_add =
        saved_group->saved_tabs()[relative_index_in_group].url();

    // Open the tab in the tabstrip and add it to the end of the group.
    const content::WebContents* const new_tab =
        SavedTabGroupUtils::OpenTabInBrowser(
            url_to_add, browser, profile_,
            WindowOpenDisposition::NEW_BACKGROUND_TAB);

    const int tab_index =
        browser->tab_strip_model()->GetIndexOfWebContents(new_tab);
    browser->tab_strip_model()->AddToExistingGroup({tab_index},
                                                   tab_group->id());
  }

  CHECK(size_t(tab_group->tab_count()) == num_tabs_in_saved_group);
}

void SavedTabGroupKeyedService::RemoveExtraTabsFromOutOfSyncLocalTabGroup(
    TabStripModel* tab_strip_model,
    TabGroup* const tab_group,
    const SavedTabGroup* const saved_group) {
  size_t num_tabs_in_saved_group = saved_group->saved_tabs().size();

  // Remove tabs from the end of the tab group to even out the number of tabs in
  // the local and saved group.
  while (tab_group->tab_count() > int(num_tabs_in_saved_group)) {
    const int last_tab = tab_group->GetLastTab().value();
    tab_strip_model->CloseWebContentsAt(
        last_tab, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  }

  CHECK(size_t(tab_group->tab_count()) == num_tabs_in_saved_group);
}

void SavedTabGroupKeyedService::UpdateWebContentsToMatchSavedTabGroupTabs(
    const TabStripModel* const tab_strip_model,
    const SavedTabGroup* const saved_group,
    const gfx::Range& tab_range) {
  for (size_t index_in_tabstrip = tab_range.start();
       index_in_tabstrip < tab_range.end(); ++index_in_tabstrip) {
    content::WebContents* const web_contents =
        tab_strip_model->GetWebContentsAt(index_in_tabstrip);
    CHECK(web_contents);

    const int saved_tab_index = index_in_tabstrip - tab_range.start();
    const SavedTabGroupTab& saved_tab =
        saved_group->saved_tabs()[saved_tab_index];

    if (saved_tab.url() != web_contents->GetLastCommittedURL()) {
      web_contents->GetController().LoadURLWithParams(
          content::NavigationController::LoadURLParams(saved_tab.url()));
    }
  }
}

std::map<content::WebContents*, base::Uuid>
SavedTabGroupKeyedService::GetWebContentsToTabGuidMappingForSavedGroup(
    const TabStripModel* const tab_strip_model,
    const SavedTabGroup* const saved_group,
    const gfx::Range& tab_range) {
  std::map<content::WebContents*, base::Uuid> web_contents_map;

  for (size_t index_in_tabstrip = tab_range.start();
       index_in_tabstrip < tab_range.end(); ++index_in_tabstrip) {
    content::WebContents* const web_contents =
        tab_strip_model->GetWebContentsAt(index_in_tabstrip);
    CHECK(web_contents);

    const int saved_tab_index = index_in_tabstrip - tab_range.start();
    const SavedTabGroupTab& saved_tab =
        saved_group->saved_tabs()[saved_tab_index];

    web_contents_map.emplace(web_contents, saved_tab.saved_tab_guid());
  }

  return web_contents_map;
}

std::map<content::WebContents*, base::Uuid>
SavedTabGroupKeyedService::GetWebContentsToTabGuidMappingForOpening(
    Browser* browser,
    const SavedTabGroup* const saved_group,
    const base::Uuid& saved_group_guid) {
  std::map<content::WebContents*, base::Uuid> web_contents;
  for (const SavedTabGroupTab& saved_tab : saved_group->saved_tabs()) {
    if (!saved_tab.url().is_valid()) {
      continue;
    }

    content::WebContents* created_contents =
        SavedTabGroupUtils::OpenTabInBrowser(
            saved_tab.url(), browser, profile_,
            WindowOpenDisposition::NEW_BACKGROUND_TAB);

    if (!created_contents) {
      continue;
    }

    web_contents.emplace(created_contents, saved_tab.saved_tab_guid());
  }
  return web_contents;
}

void SavedTabGroupKeyedService::FocusFirstTabOrWindowInOpenGroup(
    tab_groups::TabGroupId local_group_id) {
  Browser* browser_for_activation =
      SavedTabGroupUtils::GetBrowserWithTabGroupId(local_group_id);

  // Only activate the tab group's first tab, if it exists in any browser's
  // tabstrip model and it is not in the active tab in the tab group.
  CHECK(browser_for_activation);
  TabGroup* tab_group =
      browser_for_activation->tab_strip_model()->group_model()->GetTabGroup(
          local_group_id);

  absl::optional<int> first_tab = tab_group->GetFirstTab();
  absl::optional<int> last_tab = tab_group->GetLastTab();
  int active_index = browser_for_activation->tab_strip_model()->active_index();
  DCHECK(first_tab.has_value());
  DCHECK(last_tab.has_value());
  DCHECK_GT(active_index, 0);

  if (active_index >= first_tab.value() && active_index <= last_tab) {
    browser_for_activation->window()->Activate();
    return;
  }

  browser_for_activation->ActivateContents(
      browser_for_activation->tab_strip_model()->GetWebContentsAt(
          first_tab.value()));

  base::RecordAction(
      base::UserMetricsAction("TabGroups_SavedTabGroups_Focused"));
}

const TabStripModel* SavedTabGroupKeyedService::GetTabStripModelWithTabGroupId(
    const tab_groups::TabGroupId& local_group_id) {
  const Browser* const browser =
      SavedTabGroupUtils::GetBrowserWithTabGroupId(local_group_id);
  CHECK(browser);
  return browser->tab_strip_model();
}

void SavedTabGroupKeyedService::UpdateGroupVisualData(
    const base::Uuid saved_group_guid,
    const tab_groups::TabGroupId group_id) {
  TabGroup* const tab_group = SavedTabGroupUtils::GetTabGroupWithId(group_id);
  CHECK(tab_group);
  const SavedTabGroup* const saved_group = model_.Get(saved_group_guid);
  CHECK(saved_group);

  // Update the group to use the saved title and color.
  tab_groups::TabGroupVisualData visual_data(saved_group->title(),
                                             saved_group->color(),
                                             /*is_collapsed=*/false);
  tab_group->SetVisualData(visual_data, /*is_customized=*/true);
}

void SavedTabGroupKeyedService::RecordMetrics() {
  RecordSavedTabGroupMetrics();
  RecordTabGroupMetrics();
  metrics_timer_.Reset();
}

void SavedTabGroupKeyedService::RecordSavedTabGroupMetrics() {
  base::UmaHistogramCounts10000("TabGroups.SavedTabGroupCount",
                                model()->Count());

  const base::Time current_time = base::Time::Now();

  for (const SavedTabGroup& group : model()->saved_tab_groups()) {
    base::UmaHistogramCounts10000("TabGroups.SavedTabGroupTabCount",
                                  group.saved_tabs().size());

    const base::TimeDelta duration_saved =
        current_time - group.creation_time_windows_epoch_micros();
    if (!duration_saved.is_negative()) {
      base::UmaHistogramCounts1M("TabGroups.SavedTabGroupAge",
                                 duration_saved.InMinutes());
    }

    const base::TimeDelta duration_since_group_modification =
        current_time - group.update_time_windows_epoch_micros();
    if (!duration_since_group_modification.is_negative()) {
      base::UmaHistogramCounts1M("TabGroups.SavedTabGroupTimeSinceModification",
                                 duration_since_group_modification.InMinutes());
    }

    for (const SavedTabGroupTab& tab : group.saved_tabs()) {
      const base::TimeDelta duration_since_tab_modification =
          current_time - tab.update_time_windows_epoch_micros();
      if (duration_since_tab_modification.is_negative()) {
        continue;
      }

      base::UmaHistogramCounts1M(
          "TabGroups.SavedTabGroupTabTimeSinceModification",
          duration_since_tab_modification.InMinutes());
    }
  }
}

void SavedTabGroupKeyedService::RecordTabGroupMetrics() {
  int total_unsaved_groups = 0;

  for (Browser* browser : *BrowserList::GetInstance()) {
    if (profile_ != browser->profile()) {
      continue;
    }

    const TabStripModel* const tab_strip_model = browser->tab_strip_model();
    if (!tab_strip_model->SupportsTabGroups()) {
      return;
    }

    const TabGroupModel* group_model = tab_strip_model->group_model();
    CHECK(group_model);

    std::vector<tab_groups::TabGroupId> group_ids =
        group_model->ListTabGroups();

    for (tab_groups::TabGroupId group_id : group_ids) {
      if (model()->Contains(group_id)) {
        continue;
      }

      const TabGroup* const group = group_model->GetTabGroup(group_id);
      base::UmaHistogramCounts10000("TabGroups.UnsavedTabGroupTabCount",
                                    group->tab_count());
      ++total_unsaved_groups;
    }
  }

  // Record total number of non-saved tab groups in all browsers.
  base::UmaHistogramCounts10000("TabGroups.UnsavedTabGroupCount",
                                total_unsaved_groups);
}
