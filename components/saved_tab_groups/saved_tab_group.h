// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_H_
#define COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_H_

#include <memory>
#include <string>
#include <vector>

#include "base/uuid.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

// Preserves the state of a Tab group that was saved from the
// tab_group_editor_bubble_view's save toggle button. Additionally, these values
// may change if the tab groups name, color, or urls are changed from the
// tab_group_editor_bubble_view.
class SavedTabGroup {
 public:
  SavedTabGroup(
      const std::u16string& title,
      const tab_groups::TabGroupColorId& color,
      const std::vector<SavedTabGroupTab>& urls,
      absl::optional<size_t> position,
      absl::optional<base::Uuid> saved_guid = absl::nullopt,
      absl::optional<tab_groups::TabGroupId> local_group_id = absl::nullopt,
      absl::optional<base::Time> creation_time_windows_epoch_micros =
          absl::nullopt,
      absl::optional<base::Time> update_time_windows_epoch_micros =
          absl::nullopt);
  SavedTabGroup(const SavedTabGroup& other);
  ~SavedTabGroup();

  // Metadata accessors.
  const base::Uuid& saved_guid() const { return saved_guid_; }
  const absl::optional<tab_groups::TabGroupId>& local_group_id() const {
    return local_group_id_;
  }
  const base::Time& creation_time_windows_epoch_micros() const {
    return creation_time_windows_epoch_micros_;
  }
  const base::Time& update_time_windows_epoch_micros() const {
    return update_time_windows_epoch_micros_;
  }
  const std::u16string& title() const { return title_; }
  const tab_groups::TabGroupColorId& color() const { return color_; }
  const std::vector<SavedTabGroupTab>& saved_tabs() const {
    return saved_tabs_;
  }
  absl::optional<size_t> position() const { return position_; }

  std::vector<SavedTabGroupTab>& saved_tabs() { return saved_tabs_; }

  // Accessors for Tabs based on id.
  const SavedTabGroupTab* GetTab(const base::Uuid& saved_tab_guid) const;
  const SavedTabGroupTab* GetTab(const base::Token& local_tab_id) const;

  // Non const accessors for tabs based on id. this should only be used inside
  // of the model methods.
  SavedTabGroupTab* GetTab(const base::Uuid& saved_tab_guid);
  SavedTabGroupTab* GetTab(const base::Token& local_tab_id);

  // Returns the index for `tab_id` in `saved_tabs_` if it exists. Otherwise,
  // returns absl::nullopt.
  absl::optional<int> GetIndexOfTab(const base::Uuid& saved_tab_guid) const;
  absl::optional<int> GetIndexOfTab(const base::Token& local_tab_id) const;

  // Returns true if the `tab_id` was found in `saved_tabs_`.
  bool ContainsTab(const base::Uuid& saved_tab_guid) const;
  bool ContainsTab(const base::Token& tab_id) const;

  // Metadata mutators.
  SavedTabGroup& SetTitle(std::u16string title);
  SavedTabGroup& SetColor(tab_groups::TabGroupColorId color);
  SavedTabGroup& SetLocalGroupId(
      absl::optional<tab_groups::TabGroupId> tab_group_id);
  SavedTabGroup& SetUpdateTimeWindowsEpochMicros(
      base::Time update_time_windows_epoch_micros);
  SavedTabGroup& SetPosition(size_t position);

  // Tab mutators.
  // Add `tab` into its position in `saved_tabs_` if it is set. Otherwise add it
  // to the end. If the tab already exists, CHECK. If the tab was added locally
  // update the positions of all the tabs in the group. Otherwise, leave the
  // order of the group as is.
  SavedTabGroup& AddTabLocally(SavedTabGroupTab tab);
  SavedTabGroup& AddTabFromSync(SavedTabGroupTab tab);

  // Updates the tab with with `tab_id` tab.guid() with a value of `tab`. If
  // there is no tab, this function will CHECK.
  SavedTabGroup& UpdateTab(SavedTabGroupTab tab);

  // Removes a tab from `saved_tabs_` denoted by `saved_tab_guid` even if that
  // was the last tab in the group: crbug/1371959. If the tab was removed
  // locally update the positions of all tabs in the group. Otherwise, leave the
  // order of the group as is.
  SavedTabGroup& RemoveTabLocally(const base::Uuid& saved_tab_guid);
  SavedTabGroup& RemoveTabFromSync(const base::Uuid& saved_tab_guid);

  // Replaces that tab denoted by `tab_id` with value of `tab` unless the
  // replacement tab already exists. In this case we CHECK.
  SavedTabGroup& ReplaceTabAt(const base::Uuid& saved_tab_guid,
                              SavedTabGroupTab tab);

  // Moves the tab denoted by `tab_id` from its current index to the
  // `new_index`. If the tab was moved locally update the positions of all tabs
  // in the group. Otherwise, leave the order of the tabs as is. We do this
  // because sync does not guarantee all the data will be sent, in the order the
  // changes were made.
  SavedTabGroup& MoveTabLocally(const base::Uuid& saved_tab_guid,
                                size_t new_index);
  SavedTabGroup& MoveTabFromSync(const base::Uuid& saved_tab_guid,
                                 size_t new_index);

  // Merges this groups data with a specific from sync and returns the newly
  // merged specific. Side effect: Updates the values of this group.
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> MergeGroup(
      const sync_pb::SavedTabGroupSpecifics& sync_specific);

  // We should merge a group if one of the following is true:
  // 1. The data from `sync_specific` has the most recent (larger) update time.
  // 2. The `sync_specific` has the oldest (smallest) creation time.
  bool ShouldMergeGroup(
      const sync_pb::SavedTabGroupSpecifics& sync_specific) const;

  // Converts a `SavedTabGroupSpecifics` retrieved from sync into a
  // `SavedTabGroupTab`.
  static SavedTabGroup FromSpecifics(
      const sync_pb::SavedTabGroupSpecifics& specific);

  // Converts a `SavedTabGroupTab` into a `SavedTabGroupSpecifics` for sync.
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> ToSpecifics() const;

  // Returns true iff syncable data fields in `this` and `other` are equivalent.
  bool IsSyncEquivalent(const SavedTabGroup& other) const;

  // Converts tab group color ids into the sync data type for saved tab group
  // colors.
  static ::sync_pb::SavedTabGroup_SavedTabGroupColor TabGroupColorToSyncColor(
      const tab_groups::TabGroupColorId color);

  // Converts sync group colors into tab group colors ids.
  static tab_groups::TabGroupColorId SyncColorToTabGroupColor(
      const sync_pb::SavedTabGroup::SavedTabGroupColor color);

 private:
  // Moves the tab denoted by `saved_tab_guid` to the position `new_index`.
  void MoveTabImpl(const base::Uuid& saved_tab_guid, size_t new_index);

  // Insert `tab` into sorted order based on its position compared to already
  // stored tabs in its group. It should be noted that the list of tabs in each
  // group must already be in sorted order for this function to work as
  // intended. To do this, UpdateTabPositionsImpl() can be called before calling
  // this method.
  void InsertTabImpl(SavedTabGroupTab tab);

  // Updates all tab positions to match the index they are currently stored at
  // in the group at `group_index`. Does not call observers.
  void UpdateTabPositionsImpl();

  // Removes `saved_tab_guid` from this group.
  void RemoveTabImpl(const base::Uuid& saved_tab_guid);

  // The ID used to represent the group in sync.
  base::Uuid saved_guid_;

  // The ID of the tab group in the tab strip which is associated with the saved
  // tab group object. This can be null if the saved tab group is not in any tab
  // strip.
  absl::optional<tab_groups::TabGroupId> local_group_id_;

  // The title of the saved tab group.
  std::u16string title_;

  // The color of the saved tab group.
  tab_groups::TabGroupColorId color_;

  // The URLS and later webcontents (such as favicons) of the saved tab group.
  std::vector<SavedTabGroupTab> saved_tabs_;

  // The current position of the group in relation to all other saved groups.
  // A value of nullopt means that the group was not assigned a position and
  // will be assigned one when it is added into the SavedTabGroupModel.
  absl::optional<size_t> position_;

  // Timestamp for when the tab was created using windows epoch microseconds.
  base::Time creation_time_windows_epoch_micros_;

  // Timestamp for when the tab was last updated using windows epoch
  // microseconds.
  base::Time update_time_windows_epoch_micros_;
};

#endif  // COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_H_
