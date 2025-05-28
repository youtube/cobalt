// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/stats.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_visual_data.h"

namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(TabGroupLoadedEmptiness)
enum class TabGroupLoadedEmptiness {
  kNotEmpty = 0,
  kAlreadyEmpty = 1,
  kBecameEmptyAfterRemovingDuplicates = 2,
  kMaxValue = kBecameEmptyAfterRemovingDuplicates,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/tab/enums.xml:TabGroupLoadedEmptiness)
}  // namespace

namespace tab_groups {
namespace stats {
constexpr base::TimeDelta kModifiedThreshold = base::Days(30);

// Only used for desktop code that uses SavedTabGroupKeyedService. Soon to be
// deprecated.
void RecordSavedTabGroupMetrics(SavedTabGroupModel* model) {
  base::UmaHistogramCounts10000("TabGroups.SavedTabGroupCount", model->Count());

  const base::Time current_time = base::Time::Now();

  int pinned_group_count = 0;
  int active_group_count = 0;

  for (const SavedTabGroup& group : model->saved_tab_groups()) {
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

      if (duration_since_group_modification <= kModifiedThreshold) {
        ++active_group_count;
      }
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

    if (group.is_pinned()) {
      ++pinned_group_count;
    }
  }

  base::UmaHistogramCounts10000("TabGroups.SavedTabGroupPinnedCount",
                                pinned_group_count);
  base::UmaHistogramCounts10000("TabGroups.SavedTabGroupUnpinnedCount",
                                model->Count() - pinned_group_count);
  base::UmaHistogramCounts10000("TabGroups.SavedTabGroupActiveCount",
                                active_group_count);
}

void RecordTabCountMismatchOnConnect(size_t tabs_in_saved_group,
                                     size_t tabs_in_group) {
  if (tabs_in_group > tabs_in_saved_group) {
    base::UmaHistogramCounts100(
        "TabGroups.SavedTabGroups.TabCountDifference.Positive",
        tabs_in_group - tabs_in_saved_group);
  } else if (tabs_in_group < tabs_in_saved_group) {
    base::UmaHistogramCounts100(
        "TabGroups.SavedTabGroups.TabCountDifference.Negative",
        tabs_in_saved_group - tabs_in_group);
  }
}

void RecordMigrationResult(MigrationResult migration_result) {
  base::UmaHistogramEnumeration(
      "TabGroups.SavedTabGroupSyncBridge.MigrationResult", migration_result,
      MigrationResult::kCount);
}

void RecordSpecificsParseFailureCount(int parse_failure_count,
                                      int total_entries) {
  int parse_failure_percentage = 0;
  if (total_entries != 0) {
    parse_failure_percentage = base::ClampRound(
        100.f * (static_cast<float>(parse_failure_count) / total_entries));
  }

  base::UmaHistogramPercentage(
      "TabGroups.SpecificsToDataMigration.ParseFailurePercentage",
      parse_failure_percentage);
}

void RecordParsedSavedTabGroupDataCount(int parsed_entries_count,
                                        int total_entries) {
  int parse_failure_percentage = 0;
  if (total_entries != 0) {
    parse_failure_percentage = base::ClampRound(
        100.f * (static_cast<float>(total_entries - parsed_entries_count) /
                 total_entries));
  }

  base::UmaHistogramPercentage(
      "TabGroups.ParseSavedTabGroupDataEntries.ParseFailurePercentage",
      parse_failure_percentage);
}

void RecordTabGroupVisualsMetrics(
    const tab_groups::TabGroupVisualData* visual_data) {
  if (!visual_data) {
    return;
  }

  base::UmaHistogramCounts100("TabGroups.Sync.TabGroupTitleLength",
                              visual_data->title().length());
}

void RecordSharedTabGroupDataLoadFromDiskResult(
    SharedTabGroupDataLoadFromDiskResult result) {
  base::UmaHistogramEnumeration(
      "TabGroups.Shared.LoadFromDiskResult", result);
}

void RecordEmptyGroupsMetricsOnLoad(
    const std::vector<SavedTabGroup>& all_groups,
    const std::vector<SavedTabGroupTab>& all_tabs,
    const std::unordered_set<base::Uuid, base::UuidHash>&
        groups_with_filtered_tabs) {
  // Empty groups will sometimes happen because the order of sync messages are
  // not guaranteed - and clients might quit while they are missing some out of
  // order messages, so groups might be empty on load too. Record metrics so we
  // can validate that this is rare as expected, and that we don't create empty
  // groups in other ways.
  std::unordered_set<base::Uuid, base::UuidHash> empty_groups;
  for (const SavedTabGroup& group : all_groups) {
    empty_groups.emplace(group.saved_guid());
  }
  for (const SavedTabGroupTab& tab : all_tabs) {
    empty_groups.erase(tab.saved_group_guid());
  }

  for (const SavedTabGroup& group : all_groups) {
    TabGroupLoadedEmptiness metric_value = TabGroupLoadedEmptiness::kNotEmpty;
    if (empty_groups.contains(group.saved_guid())) {
      metric_value =
          groups_with_filtered_tabs.contains(group.saved_guid())
              ? TabGroupLoadedEmptiness::kBecameEmptyAfterRemovingDuplicates
              : TabGroupLoadedEmptiness::kAlreadyEmpty;
    }
    base::UmaHistogramEnumeration(
        "TabGroups.SavedTabGroups.TabGroupLoadedEmptiness", metric_value);
  }
}

void RecordEmptyGroupsMetricsOnGroupAddedLocally(const SavedTabGroup& group,
                                                 bool model_is_loaded) {
  // This method is used to load the model from disk. Avoid recording metrics
  // during load so we don't double count with the previous session.
  if (!model_is_loaded) {
    return;
  }

  base::UmaHistogramBoolean("TabGroups.Sync.AddedGroupIsEmptyLocally",
                            group.saved_tabs().empty());
}

void RecordEmptyGroupsMetricsOnGroupAddedFromSync(const SavedTabGroup& group,
                                                  bool model_is_loaded) {
  // Avoid recording metrics during load so we don't double count with the
  // previous session. This method is currently not called during load, but that
  // could change.
  if (!model_is_loaded) {
    return;
  }

  base::UmaHistogramBoolean("TabGroups.Sync.AddedGroupIsEmptyFromSync",
                            group.saved_tabs().empty());
}

void RecordEmptyGroupsMetricsOnTabAddedLocally(const SavedTabGroup& group,
                                               const SavedTabGroupTab& tab,
                                               bool model_is_loaded) {
  // Avoid recording metrics during load so we don't double count with the
  // previous session. This method is currently not called during load, but that
  // could change.
  if (!model_is_loaded) {
    return;
  }

  // The tab must not be already in the group; otherwise we can't tell if the
  // group was empty before (as the tab may have been replaced).
  CHECK(!group.ContainsTab(tab.saved_tab_guid()));

  base::UmaHistogramBoolean("TabGroups.Sync.TabAddedToEmptyGroupLocally",
                            group.saved_tabs().empty());
}

void RecordEmptyGroupsMetricsOnTabAddedFromSync(const SavedTabGroup& group,
                                                const SavedTabGroupTab& tab,
                                                bool model_is_loaded) {
  // This method is used to load the model from disk. Avoid recording metrics
  // during load so we don't double count with the previous session.
  if (!model_is_loaded) {
    return;
  }

  // The tab must not be already in the group; otherwise we can't tell if the
  // group was empty before (as the tab may have been replaced).
  CHECK(!group.ContainsTab(tab.saved_tab_guid()));

  base::UmaHistogramBoolean("TabGroups.Sync.TabAddedToEmptyGroupFromSync",
                            group.saved_tabs().empty());
}

void RecordSharedGroupTitleSanitization(
    bool use_url_as_title,
    TitleSanitizationType title_sanitization_type) {
  std::string histogram;
  switch (title_sanitization_type) {
    case TitleSanitizationType::kAddTab:
      histogram = "TabGroups.Shared.UseUrlForTitle.AddTab";
      break;
    case TitleSanitizationType::kNavigateTab:
      histogram = "TabGroups.Shared.UseUrlForTitle.NavigateTab";
      break;
    case TitleSanitizationType::kShareTabGroup:
      histogram = "TabGroups.Shared.UseUrlForTitle.ShareTabGroup";
      break;
  }
  base::UmaHistogramBoolean(histogram, use_url_as_title);
}
}  // namespace stats
}  // namespace tab_groups
