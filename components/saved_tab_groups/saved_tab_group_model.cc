// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/saved_tab_group_model.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
void RecordGroupDeletedMetric(const SavedTabGroup* removed_group) {
  const base::TimeDelta duration_saved =
      base::Time::Now() - removed_group->creation_time_windows_epoch_micros();

  base::UmaHistogramCounts1M("TabGroups.SavedTabGroupLifespan",
                             duration_saved.InMinutes());

  base::RecordAction(
      base::UserMetricsAction("TabGroups_SavedTabGroups_Deleted"));
}
}  // anonymous namespace

SavedTabGroupModel::SavedTabGroupModel() = default;
SavedTabGroupModel::~SavedTabGroupModel() = default;

absl::optional<int> SavedTabGroupModel::GetIndexOf(
    tab_groups::TabGroupId tab_group_id) const {
  for (size_t i = 0; i < saved_tab_groups_.size(); i++) {
    if (saved_tab_groups_[i].local_group_id() == tab_group_id)
      return i;
  }

  return absl::nullopt;
}

absl::optional<int> SavedTabGroupModel::GetIndexOf(const base::Uuid& id) const {
  for (size_t i = 0; i < saved_tab_groups_.size(); i++) {
    if (saved_tab_groups_[i].saved_guid() == id)
      return i;
  }

  return absl::nullopt;
}

const SavedTabGroup* SavedTabGroupModel::Get(const base::Uuid& id) const {
  absl::optional<int> index = GetIndexOf(id);
  if (!index.has_value()) {
    return nullptr;
  }

  return &saved_tab_groups_[index.value()];
}

const SavedTabGroup* SavedTabGroupModel::Get(
    const tab_groups::TabGroupId local_group_id) const {
  absl::optional<int> index = GetIndexOf(local_group_id);
  if (!index.has_value())
    return nullptr;

  return &saved_tab_groups_[index.value()];
}

void SavedTabGroupModel::Add(SavedTabGroup saved_group) {
  base::Uuid group_guid = saved_group.saved_guid();
  CHECK(!Contains(group_guid));

  // Give a default position to groups if it is not already set.
  if (!saved_group.position().has_value()) {
    saved_group.SetPosition(Count());
  }

  InsertGroupImpl(std::move(saved_group));

  for (auto& observer : observers_) {
    observer.SavedTabGroupAddedLocally(Get(group_guid)->saved_guid());
  }
}

void SavedTabGroupModel::Remove(const tab_groups::TabGroupId tab_group_id) {
  if (!Contains(tab_group_id))
    return;

  const int index = GetIndexOf(tab_group_id).value();
  base::Uuid removed_guid = Get(tab_group_id)->saved_guid();
  std::unique_ptr<SavedTabGroup> removed_group = RemoveImpl(index);

  UpdateGroupPositionsImpl();
  for (auto& observer : observers_) {
    observer.SavedTabGroupRemovedLocally(removed_group.get());
  }

  RecordGroupDeletedMetric(removed_group.get());
}

void SavedTabGroupModel::Remove(const base::Uuid& id) {
  if (!Contains(id))
    return;

  const int index = GetIndexOf(id).value();
  base::Uuid removed_guid = Get(id)->saved_guid();
  std::unique_ptr<SavedTabGroup> removed_group = RemoveImpl(index);

  UpdateGroupPositionsImpl();
  for (auto& observer : observers_) {
    observer.SavedTabGroupRemovedLocally(removed_group.get());
  }

  RecordGroupDeletedMetric(removed_group.get());
}

void SavedTabGroupModel::UpdateVisualData(
    tab_groups::TabGroupId tab_group_id,
    const tab_groups::TabGroupVisualData* visual_data) {
  if (!Contains(tab_group_id))
    return;

  const absl::optional<int> index = GetIndexOf(tab_group_id);
  UpdateVisualDataImpl(index.value(), visual_data);
  base::Uuid updated_guid = Get(tab_group_id)->saved_guid();
  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedLocally(updated_guid);
  }
}

void SavedTabGroupModel::UpdateVisualData(
    const base::Uuid& id,
    const tab_groups::TabGroupVisualData* visual_data) {
  if (!Contains(id))
    return;

  const absl::optional<int> index = GetIndexOf(id);
  UpdateVisualDataImpl(index.value(), visual_data);
  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedLocally(id);
  }
}

void SavedTabGroupModel::AddedFromSync(SavedTabGroup saved_group) {
  base::Uuid group_guid = saved_group.saved_guid();
  if (Contains(group_guid))
    return;

  InsertGroupImpl(std::move(saved_group));

  for (auto& observer : observers_) {
    observer.SavedTabGroupAddedFromSync(Get(group_guid)->saved_guid());
  }
}

void SavedTabGroupModel::RemovedFromSync(
    const tab_groups::TabGroupId tab_group_id) {
  if (!Contains(tab_group_id))
    return;

  const absl::optional<int> index = GetIndexOf(tab_group_id);
  base::Uuid removed_guid = Get(tab_group_id)->saved_guid();
  std::unique_ptr<SavedTabGroup> removed_group = RemoveImpl(index.value());
  for (auto& observer : observers_) {
    observer.SavedTabGroupRemovedFromSync(removed_group.get());
  }
}

void SavedTabGroupModel::RemovedFromSync(const base::Uuid& id) {
  if (!Contains(id))
    return;

  const absl::optional<int> index = GetIndexOf(id);
  base::Uuid removed_guid = Get(id)->saved_guid();
  std::unique_ptr<SavedTabGroup> removed_group = RemoveImpl(index.value());
  for (auto& observer : observers_) {
    observer.SavedTabGroupRemovedFromSync(removed_group.get());
  }
}

void SavedTabGroupModel::UpdatedVisualDataFromSync(
    tab_groups::TabGroupId tab_group_id,
    const tab_groups::TabGroupVisualData* visual_data) {
  if (!Contains(tab_group_id))
    return;

  const absl::optional<int> index = GetIndexOf(tab_group_id);
  UpdateVisualDataImpl(index.value(), visual_data);
  base::Uuid updated_guid = Get(tab_group_id)->saved_guid();
  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedFromSync(updated_guid);
  }
}

void SavedTabGroupModel::UpdatedVisualDataFromSync(
    const base::Uuid& id,
    const tab_groups::TabGroupVisualData* visual_data) {
  if (!Contains(id))
    return;

  const absl::optional<int> index = GetIndexOf(id);
  UpdateVisualDataImpl(index.value(), visual_data);
  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedFromSync(id);
  }
}

SavedTabGroup* SavedTabGroupModel::GetGroupContainingTab(
    const base::Uuid& saved_tab_guid) {
  for (auto& saved_group : saved_tab_groups_) {
    if (saved_group.ContainsTab(saved_tab_guid))
      return &saved_group;
  }

  return nullptr;
}

SavedTabGroup* SavedTabGroupModel::GetGroupContainingTab(
    const base::Token& local_tab_id) {
  for (auto& saved_group : saved_tab_groups_) {
    if (saved_group.ContainsTab(local_tab_id))
      return &saved_group;
  }

  return nullptr;
}

void SavedTabGroupModel::AddTabToGroupLocally(const base::Uuid& group_id,
                                              SavedTabGroupTab tab) {
  if (!Contains(group_id)) {
    return;
  }

  const base::Uuid tab_id = tab.saved_tab_guid();
  absl::optional<int> group_index = GetIndexOf(group_id);
  saved_tab_groups_[group_index.value()].AddTabLocally(tab);

  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedLocally(group_id, tab_id);
  }

  base::RecordAction(
      base::UserMetricsAction("TabGroups_SavedTabGroups_TabAdded"));
}

void SavedTabGroupModel::AddTabToGroupFromSync(const base::Uuid& group_id,
                                               SavedTabGroupTab tab) {
  if (!Contains(group_id)) {
    return;
  }

  const base::Uuid tab_id = tab.saved_tab_guid();
  absl::optional<int> group_index = GetIndexOf(group_id);
  saved_tab_groups_[group_index.value()].AddTabFromSync(tab);

  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedFromSync(group_id, tab_id);
  }
}

void SavedTabGroupModel::UpdateTabInGroup(const base::Uuid& group_id,
                                          SavedTabGroupTab tab) {
  absl::optional<int> group_index = GetIndexOf(group_id);
  CHECK(group_index.has_value());

  const SavedTabGroupTab* const old_tab =
      saved_tab_groups_[group_index.value()].GetTab(tab.saved_tab_guid());

  if (old_tab->url() != tab.url()) {
    base::RecordAction(
        base::UserMetricsAction("TabGroups_SavedTabGroups_TabNavigated"));
  }

  saved_tab_groups_[group_index.value()].UpdateTab(tab);

  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedLocally(group_id, tab.saved_tab_guid());
  }
}

void SavedTabGroupModel::UpdateLocalTabId(
    const base::Uuid& group_id,
    SavedTabGroupTab tab,
    absl::optional<base::Token> local_id) {
  absl::optional<int> group_index = GetIndexOf(group_id);
  CHECK(group_index.has_value());
  tab.SetLocalTabID(local_id);
  saved_tab_groups_[group_index.value()].UpdateTab(tab);
}

void SavedTabGroupModel::RemoveTabFromGroupLocally(const base::Uuid& group_id,
                                                   const base::Uuid& tab_id) {
  if (!Contains(group_id)) {
    return;
  }

  absl::optional<int> index = GetIndexOf(group_id);
  SavedTabGroup group = saved_tab_groups_[index.value()];

  if (!group.ContainsTab(tab_id)) {
    return;
  }

  // Remove the group from the model if the last tab will be removed from it.
  if (group.saved_tabs().size() == 1) {
    Remove(group_id);
    return;
  }

  // TODO(crbug/1401965): Convert all methods to pass ids by value to prevent
  // UAFs. Also removes the need for a separate copy variable.
  const base::Uuid copy_tab_id = tab_id;
  saved_tab_groups_[index.value()].RemoveTabLocally(tab_id);

  // TODO(dljames): Update to use SavedTabGroupRemoveLocally and update the API
  // to pass a group_id and an optional tab_id.
  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedLocally(group_id, copy_tab_id);
  }

  base::RecordAction(
      base::UserMetricsAction("TabGroups_SavedTabGroups_TabRemoved"));
}

void SavedTabGroupModel::RemoveTabFromGroupFromSync(const base::Uuid& group_id,
                                                    const base::Uuid& tab_id) {
  if (!Contains(group_id)) {
    return;
  }

  absl::optional<int> index = GetIndexOf(group_id);
  SavedTabGroup group = saved_tab_groups_[index.value()];

  if (!group.ContainsTab(tab_id)) {
    return;
  }

  // Remove the group from the model if the last tab will be removed from it.
  if (group.saved_tabs().size() == 1) {
    RemovedFromSync(group_id);
    return;
  }

  // Copy `tab_id` to prevent uaf when ungrouping a saved tab: crbug/1401965.
  const base::Uuid copy_tab_id = tab_id;
  saved_tab_groups_[index.value()].RemoveTabFromSync(tab_id);

  // TODO(dljames): Update to use SavedTabGroupRemoveFromSync and update the API
  // to pass a group_id and an optional tab_id.
  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedFromSync(group_id, copy_tab_id);
  }
}

void SavedTabGroupModel::MoveTabInGroupTo(const base::Uuid& group_id,
                                          const base::Uuid& tab_id,
                                          int new_index) {
  if (!Contains(group_id))
    return;

  // Copy `tab_id` to prevent uaf when ungrouping a saved tab: crbug/1401965.
  const base::Uuid copy_tab_id = tab_id;
  absl::optional<int> index = GetIndexOf(group_id);
  saved_tab_groups_[index.value()].MoveTabLocally(tab_id, new_index);

  for (auto& observer : observers_) {
    // TODO(crbug/1459730): Consider further optimizations.
    observer.SavedTabGroupTabsReorderedLocally(group_id);
  }
}

std::unique_ptr<sync_pb::SavedTabGroupSpecifics> SavedTabGroupModel::MergeGroup(
    const sync_pb::SavedTabGroupSpecifics& sync_specific) {
  const base::Uuid& group_id = base::Uuid::ParseLowercase(sync_specific.guid());

  DCHECK(Contains(group_id));

  const int index = GetIndexOf(group_id).value();
  const int preferred_index = sync_specific.group().position();

  saved_tab_groups_[index].MergeGroup(std::move(sync_specific));

  if (index != preferred_index) {
    const int num_groups = Count();
    const int new_index =
        preferred_index < num_groups ? preferred_index : num_groups - 1;
    ReorderGroupFromSync(group_id, std::max(new_index, 0));
  }

  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedFromSync(group_id);
  }

  return Get(group_id)->ToSpecifics();
}

std::unique_ptr<sync_pb::SavedTabGroupSpecifics> SavedTabGroupModel::MergeTab(
    const sync_pb::SavedTabGroupSpecifics& sync_specific) {
  const base::Uuid& group_guid =
      base::Uuid::ParseLowercase(sync_specific.tab().group_guid());

  const base::Uuid& tab_guid = base::Uuid::ParseLowercase(sync_specific.guid());
  SavedTabGroup* const group = GetGroupContainingTab(tab_guid);
  CHECK(group);

  const absl::optional<int> index = group->GetIndexOfTab(tab_guid);
  const int preferred_index = sync_specific.tab().position();

  group->GetTab(tab_guid)->MergeTab(std::move(sync_specific));

  if (index != preferred_index) {
    const int num_tabs = group->saved_tabs().size();
    const int new_index =
        preferred_index < num_tabs ? preferred_index : num_tabs - 1;
    group->MoveTabFromSync(tab_guid, std::max(new_index, 0));
  }

  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedFromSync(group_guid, tab_guid);
  }

  return group->GetTab(tab_guid)->ToSpecifics();
}

void SavedTabGroupModel::ReorderGroupLocally(const base::Uuid& id,
                                             int new_index) {
  ReorderGroupImpl(id, new_index);
  UpdateGroupPositionsImpl();
  for (auto& observer : observers_) {
    observer.SavedTabGroupReorderedLocally();
  }
}

void SavedTabGroupModel::ReorderGroupFromSync(const base::Uuid& id,
                                              int new_index) {
  ReorderGroupImpl(id, new_index);
  for (auto& observer : observers_) {
    observer.SavedTabGroupReorderedFromSync();
  }
}

std::vector<sync_pb::SavedTabGroupSpecifics>
SavedTabGroupModel::LoadStoredEntries(
    std::vector<sync_pb::SavedTabGroupSpecifics> entries) {
  std::vector<SavedTabGroupTab> tabs;
  std::vector<sync_pb::SavedTabGroupSpecifics> tabs_missing_groups;

  // `entries` is not ordered such that groups are guaranteed to be
  // at the front of the vector. As such, we can run into the case where we
  // try to add a tab to a group that does not exist for us yet.
  for (sync_pb::SavedTabGroupSpecifics proto : entries) {
    if (proto.has_group()) {
      Add(SavedTabGroup::FromSpecifics(proto));
    } else {
      tabs.emplace_back(SavedTabGroupTab::FromSpecifics(proto));
    }
  }

  UpdateGroupPositionsImpl();

  for (const SavedTabGroupTab& tab : tabs) {
    absl::optional<int> index = GetIndexOf(tab.saved_group_guid());
    if (!index.has_value()) {
      tabs_missing_groups.emplace_back(std::move(*tab.ToSpecifics()));
    } else {
      base::Uuid group_id = tab.saved_group_guid();
      AddTabToGroupFromSync(group_id, std::move(tab));
    }
  }

  is_loaded_ = true;
  for (auto& observer : observers_) {
    observer.SavedTabGroupModelLoaded();
  }

  return tabs_missing_groups;
}

void SavedTabGroupModel::OnGroupClosedInTabStrip(
    const tab_groups::TabGroupId& tab_group_id) {
  const absl::optional<int> index = GetIndexOf(tab_group_id);
  if (!index.has_value())
    return;

  SavedTabGroup& saved_group = saved_tab_groups_[index.value()];
  saved_group.SetLocalGroupId(absl::nullopt);

  for (auto& observer : observers_) {
    observer.SavedTabGroupLocalIdChanged(saved_group.saved_guid());
  }

  base::RecordAction(
      base::UserMetricsAction("TabGroups_SavedTabGroups_Closed"));
}

void SavedTabGroupModel::OnGroupOpenedInTabStrip(
    const base::Uuid& id,
    const tab_groups::TabGroupId& tab_group_id) {
  const absl::optional<int> index = GetIndexOf(id);
  CHECK(index.has_value());
  CHECK_GE(index.value(), 0);

  SavedTabGroup& saved_group = saved_tab_groups_[index.value()];
  saved_group.SetLocalGroupId(tab_group_id);

  for (auto& observer : observers_) {
    observer.SavedTabGroupLocalIdChanged(saved_group.saved_guid());
  }
}

void SavedTabGroupModel::AddObserver(SavedTabGroupModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SavedTabGroupModel::RemoveObserver(SavedTabGroupModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SavedTabGroupModel::ReorderGroupImpl(const base::Uuid& id, int new_index) {
  DCHECK_GE(new_index, 0);
  DCHECK_LT(new_index, Count());

  absl::optional<int> index = GetIndexOf(id);
  CHECK(index.has_value());
  CHECK_GE(index.value(), 0);

  SavedTabGroup group = saved_tab_groups_[index.value()];

  saved_tab_groups_.erase(saved_tab_groups_.begin() + index.value());
  saved_tab_groups_.emplace(saved_tab_groups_.begin() + new_index,
                            std::move(group));
}

void SavedTabGroupModel::UpdateGroupPositionsImpl() {
  for (size_t i = 0; i < saved_tab_groups_.size(); ++i) {
    saved_tab_groups_[i].SetPosition(i);
  }
}

void SavedTabGroupModel::InsertGroupImpl(const SavedTabGroup& group) {
  // We can always safely insert the first group.
  if (saved_tab_groups_.empty()) {
    saved_tab_groups_.emplace_back(std::move(group));
    return;
  }

  CHECK(group.position().has_value());

  // Because saved_tab_groups_ must be in sorted order, we can immediately place
  // the group at the end of the vector if `group` is the largest
  // element we have seen yet.
  if (saved_tab_groups_[saved_tab_groups_.size() - 1].position() <
      group.position()) {
    saved_tab_groups_.emplace_back(std::move(group));
    return;
  }

  // Insert `group` in front of an element if one of these criteria
  // are met:
  // 1. The current index is larger than `group`.
  // 2. The current index has the same position as `group` and is not
  // the most recently updated position.
  for (size_t index = 0; index < saved_tab_groups_.size(); ++index) {
    const SavedTabGroup& curr_group = saved_tab_groups_[index];
    bool curr_position_larger = curr_group.position() > group.position();
    bool curr_position_same = curr_group.position() == group.position();
    bool curr_position_least_recently_updated =
        curr_group.update_time_windows_epoch_micros() <=
        group.update_time_windows_epoch_micros();

    if (curr_position_larger ||
        (curr_position_same && curr_position_least_recently_updated)) {
      saved_tab_groups_.insert(saved_tab_groups_.begin() + index,
                               std::move(group));
      return;
    }
  }

  // This can happen when the last element of the vector has the same position
  // as `group` and was more recently updated.
  saved_tab_groups_.emplace_back(std::move(group));
}

std::unique_ptr<SavedTabGroup> SavedTabGroupModel::RemoveImpl(int index) {
  CHECK_GE(index, 0);
  std::unique_ptr<SavedTabGroup> removed_group =
      std::make_unique<SavedTabGroup>(std::move(saved_tab_groups_[index]));
  saved_tab_groups_.erase(saved_tab_groups_.begin() + index);
  return removed_group;
}

void SavedTabGroupModel::UpdateVisualDataImpl(
    int index,
    const tab_groups::TabGroupVisualData* visual_data) {
  SavedTabGroup& saved_group = saved_tab_groups_[index];
  if (saved_group.title() == visual_data->title() &&
      saved_group.color() == visual_data->color())
    return;

  saved_group.SetTitle(visual_data->title());
  saved_group.SetColor(visual_data->color());
}
