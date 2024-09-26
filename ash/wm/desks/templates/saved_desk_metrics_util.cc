// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "ash/public/cpp/desk_template.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/restore_data.h"
#include "components/desks_storage/core/desk_model.h"

namespace ash {

enum class MetricsAction {
  kDelete = 0,
  kLaunch,
  kNew,
  kReplace,
  kTabCount,
  kWindowCount,
  kWindowAndTabCount,
  kUnsupportedAppDialogShow
};

const char* GetHistogramName(DeskTemplateType type,
                             MetricsAction metrics_action_name) {
  switch (type) {
    case DeskTemplateType::kTemplate:
      switch (metrics_action_name) {
        case MetricsAction::kDelete:
          return kDeleteTemplateHistogramName;
        case MetricsAction::kLaunch:
          return kLaunchTemplateHistogramName;
        case MetricsAction::kNew:
          return kNewTemplateHistogramName;
        case MetricsAction::kReplace:
          return kReplaceTemplateHistogramName;
        case MetricsAction::kTabCount:
          return kTemplateTabCountHistogramName;
        case MetricsAction::kWindowCount:
          return kTemplateWindowCountHistogramName;
        case MetricsAction::kWindowAndTabCount:
          return kTemplateWindowAndTabCountHistogramName;
        case MetricsAction::kUnsupportedAppDialogShow:
          return kTemplateUnsupportedAppDialogShowHistogramName;
      }
    case DeskTemplateType::kSaveAndRecall:
      switch (metrics_action_name) {
        case MetricsAction::kDelete:
          return kDeleteSaveAndRecallHistogramName;
        case MetricsAction::kLaunch:
          return kLaunchSaveAndRecallHistogramName;
        case MetricsAction::kNew:
          return kNewSaveAndRecallHistogramName;
        case MetricsAction::kReplace:
          return kReplaceSaveAndRecallHistogramName;
        case MetricsAction::kTabCount:
          return kSaveAndRecallTabCountHistogramName;
        case MetricsAction::kWindowCount:
          return kSaveAndRecallWindowCountHistogramName;
        case MetricsAction::kWindowAndTabCount:
          return kSaveAndRecallWindowAndTabCountHistogramName;
        case MetricsAction::kUnsupportedAppDialogShow:
          return kSaveAndRecallUnsupportedAppDialogShowHistogramName;
      }
    case DeskTemplateType::kFloatingWorkspace:
      switch (metrics_action_name) {
        case MetricsAction::kLaunch:
          return kLaunchFloatingWorkspaceHistogramName;
        case MetricsAction::kTabCount:
          return kFloatingWorkspaceTabCountHistogramName;
        case MetricsAction::kWindowCount:
          return kFloatingWorkspaceWindowCountHistogramName;
        case MetricsAction::kWindowAndTabCount:
          return kFloatingWorkspaceWindowAndTabCountHistogramName;
        default:
          return nullptr;
      }
    case DeskTemplateType::kUnknown:
      return nullptr;
  }
}

void RecordLoadSavedDeskLibraryHistogram() {
  base::UmaHistogramBoolean(kLoadTemplateGridHistogramName, true);
}

void RecordDeleteSavedDeskHistogram(DeskTemplateType type) {
  if (const char* metrics_name =
          GetHistogramName(type, MetricsAction::kDelete)) {
    base::UmaHistogramBoolean(metrics_name, true);
  }
}

void RecordLaunchSavedDeskHistogram(DeskTemplateType type) {
  if (const char* metrics_name =
          GetHistogramName(type, MetricsAction::kLaunch)) {
    base::UmaHistogramBoolean(metrics_name, true);
  }
}

void RecordNewSavedDeskHistogram(DeskTemplateType type) {
  if (const char* metrics_name = GetHistogramName(type, MetricsAction::kNew)) {
    base::UmaHistogramBoolean(metrics_name, true);
  }
}

void RecordReplaceSavedDeskHistogram(DeskTemplateType type) {
  if (const char* metrics_name =
          GetHistogramName(type, MetricsAction::kReplace)) {
    base::UmaHistogramBoolean(metrics_name, true);
  }
}

void RecordAddOrUpdateTemplateStatusHistogram(
    desks_storage::DeskModel::AddOrUpdateEntryStatus status) {
  base::UmaHistogramEnumeration(kAddOrUpdateTemplateStatusHistogramName,
                                status);
}

void RecordUserSavedDeskCountHistogram(DeskTemplateType type,
                                       size_t entry_count,
                                       size_t max_entry_count) {
  switch (type) {
    case DeskTemplateType::kTemplate:
      UMA_HISTOGRAM_EXACT_LINEAR(kUserTemplateCountHistogramName, entry_count,
                                 max_entry_count);
      break;
    case DeskTemplateType::kSaveAndRecall:
      UMA_HISTOGRAM_EXACT_LINEAR(kUserSaveAndRecallCountHistogramName,
                                 entry_count, max_entry_count);

      break;
    case DeskTemplateType::kFloatingWorkspace:
    case DeskTemplateType::kUnknown:
      break;
  }
}

void RecordWindowAndTabCountHistogram(const DeskTemplate& desk_template) {
  const app_restore::RestoreData* restore_data =
      desk_template.desk_restore_data();
  DCHECK(restore_data);

  int window_count = 0;
  int tab_count = 0;
  int total_count = 0;

  const auto& launch_list = restore_data->app_id_to_launch_list();
  for (const auto& iter : launch_list) {
    // Since apps aren't guaranteed to have the url field set up correctly, this
    // is necessary to ensure things are not double-counted.
    if (iter.first != app_constants::kChromeAppId) {
      ++window_count;
      ++total_count;
      continue;
    }

    for (const auto& window_iter : iter.second) {
      const absl::optional<std::vector<GURL>>& urls = window_iter.second->urls;
      if (!urls || urls->empty())
        continue;

      ++window_count;
      tab_count += urls->size();
      total_count += urls->size();
    }
  }

  if (const char* window_count_metrics_name =
          GetHistogramName(desk_template.type(), MetricsAction::kWindowCount)) {
    base::UmaHistogramCounts100(window_count_metrics_name, window_count);
  }
  if (const char* tab_count_metrics_name =
          GetHistogramName(desk_template.type(), MetricsAction::kTabCount)) {
    base::UmaHistogramCounts100(tab_count_metrics_name, tab_count);
  }

  if (const char* window_and_tab_count_metrics_name = GetHistogramName(
          desk_template.type(), MetricsAction::kWindowAndTabCount)) {
    base::UmaHistogramCounts100(window_and_tab_count_metrics_name, total_count);
  }
}

void RecordUnsupportedAppDialogShowHistogram(DeskTemplateType type) {
  if (const char* metrics_name =
          GetHistogramName(type, MetricsAction::kUnsupportedAppDialogShow)) {
    base::UmaHistogramBoolean(metrics_name, true);
  }
}

void RecordTimeBetweenSaveAndRecall(base::TimeDelta duration) {
  // Constants for the histogram. Do not change these values without also
  // changing the histogram name.
  constexpr size_t bucket_count = 50;
  constexpr base::TimeDelta min_bucket = base::Seconds(1);
  constexpr base::TimeDelta max_bucket = base::Hours(24 * 7);  // One week.

  // Lazily construct the histogram.
  static auto* histogram = base::Histogram::FactoryGet(
      kTimeBetweenSaveAndRecallHistogramName, min_bucket.InSeconds(),
      max_bucket.InSeconds(), bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);

  histogram->Add(duration.InSeconds());
}

}  // namespace ash
