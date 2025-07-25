// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_ROW_VIEW_INFO_H_
#define CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_ROW_VIEW_INFO_H_

#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/download/download_bubble_info.h"
#include "chrome/browser/ui/download/download_item_mode.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"

namespace offline_item_collection {
class ContentId;
}

// Interface for observers of changes to a download row
class DownloadBubbleRowViewInfoObserver : public base::CheckedObserver {
 public:
  DownloadBubbleRowViewInfoObserver();
  ~DownloadBubbleRowViewInfoObserver() override;

  // Called whenever the info changes
  virtual void OnInfoChanged() {}

  // Called when the download changes state.
  virtual void OnDownloadStateChanged(
      download::DownloadItem::DownloadState old_state,
      download::DownloadItem::DownloadState new_state) {}

  // Called when the download is destroyed.
  virtual void OnDownloadDestroyed(
      const offline_items_collection::ContentId& id) {}
};

// Info class for a DownloadBubbleRowView
class DownloadBubbleRowViewInfo
    : public DownloadBubbleInfo<DownloadBubbleRowViewInfoObserver>,
      public DownloadUIModel::Delegate {
 public:
  struct QuickAction {
    DownloadCommands::Command command;
    std::u16string hover_text;
    raw_ptr<const gfx::VectorIcon> icon = nullptr;
    QuickAction(DownloadCommands::Command command,
                const std::u16string& hover_text,
                const gfx::VectorIcon* icon);
  };

  explicit DownloadBubbleRowViewInfo(DownloadUIModel::DownloadUIModelPtr model);
  ~DownloadBubbleRowViewInfo() override;

  // Accessors
  DownloadUIModel* model() const { return model_.get(); }
  download::DownloadItemMode mode() const {
    return download::GetDesiredDownloadItemMode(model_.get());
  }
  const gfx::VectorIcon* icon_override() const { return icon_override_; }
  ui::ColorId secondary_color() const { return secondary_color_; }
  ui::ColorId secondary_text_color() const {
    return secondary_text_color_.value_or(secondary_color_);
  }
  const std::vector<QuickAction>& quick_actions() const {
    return quick_actions_;
  }
  bool main_button_enabled() const { return main_button_enabled_; }
  bool has_subpage() const { return has_subpage_; }
  absl::optional<DownloadCommands::Command> primary_button_command() const {
    return primary_button_command_;
  }
  bool has_progress_bar() const { return has_progress_bar_; }
  bool is_progress_bar_looping() const { return is_progress_bar_looping_; }

  void SetQuickActionsForTesting(const std::vector<QuickAction>& actions);

 private:
  // Overrides DownloadUIModel::Delegate:
  void OnDownloadOpened() override;
  void OnDownloadUpdated() override;
  void OnDownloadDestroyed(
      const offline_items_collection::ContentId& id) override;

  // Update the current state to reflect `model_`.
  void PopulateFromModel();
  void PopulateForInProgressOrComplete();
  void PopulateForInterrupted(offline_items_collection::FailState fail_state);
  void PopulateForCancelled();
  void PopulateForTailoredWarning();
  void PopulateForFileTypeWarningNoSafeBrowsing();

  void PopulateSuspiciousUiPattern();
  void PopulateDangerousUiPattern();

  // Clear all fields. This helps keep the Populate* methods focused on only
  // non-default settings.
  void Reset();

  DownloadUIModel::DownloadUIModelPtr model_;
  download::DownloadItem::DownloadState state_ =
      download::DownloadItem::IN_PROGRESS;

  // Information for displaying the row. This must all be cleared in Reset() or
  // updates will keep attributes of the previously displayed state.

  // This is non-null if the row should display an icon other than the system
  // icon for the filetype.
  raw_ptr<const gfx::VectorIcon> icon_override_ = nullptr;
  // kColorAlertHighSeverity, kColorAlertMediumSeverityIcon, or
  // kColorSecondaryForeground
  ui::ColorId secondary_color_ = ui::kColorSecondaryForeground;
  // Color used for alert text, which may be different from |secondary_color|,
  // used for icons. If this is nullopt, |secondary_color| will be used for
  // text.
  absl::optional<ui::ColorId> secondary_text_color_ = absl::nullopt;
  // List of quick actions
  std::vector<QuickAction> quick_actions_;
  // Whether the main button (clicking the row itself) should be enabled. When
  // true, the main button will either:
  // - Open the subpage, if it exists
  // - Open the download, if no subpage exists
  bool main_button_enabled_ = true;
  // Whether this row has a subpage.
  bool has_subpage_ = false;
  // The command for the primary button (the button always displayed over the
  // row).
  absl::optional<DownloadCommands::Command> primary_button_command_ =
      absl::nullopt;
  // Whether the row has a progress bar
  bool has_progress_bar_ = false;
  bool is_progress_bar_looping_ = false;
};

#endif  // CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_ROW_VIEW_INFO_H_
