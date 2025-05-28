// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_RECENT_ACTIVITY_BUBBLE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_RECENT_ACTIVITY_BUBBLE_DIALOG_VIEW_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/collaboration/public/messaging/activity_log.h"
#include "components/favicon_base/favicon_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class WebContents;
}  // namespace content

class Profile;

class RecentActivityRowView;
class RecentActivityRowImageView;

using collaboration::messaging::ActivityLogItem;

DECLARE_ELEMENT_IDENTIFIER_VALUE(kRecentActivityBubbleDialogId);

// The bubble dialog view housing the Shared Tab Group Recent Activity.
// Shows at most kMaxNumberRows of the activity_log parameter.
class RecentActivityBubbleDialogView : public LocationBarBubbleDelegateView {
  METADATA_HEADER(RecentActivityBubbleDialogView, LocationBarBubbleDelegateView)

 public:
  RecentActivityBubbleDialogView(View* anchor_view,
                                 content::WebContents* web_contents,
                                 std::optional<int> current_tab_activity_index,
                                 std::vector<ActivityLogItem> activity_log,
                                 Profile* profile);
  ~RecentActivityBubbleDialogView() override;

  // The maximum number of rows that can be displayed in this dialog.
  static constexpr int kMaxNumberRows = 5;

  // Creates a state indicating there is no activity to show.
  void CreateEmptyState();

  // Creates a view containing the single most recent tab activity.
  void CreateTabActivity();

  // Creates a view containing the most recent activity for the group.
  void CreateGroupActivity();

  // Returns the row's view at the given index. This will look in both
  // the tab activity container and the group activity container.
  RecentActivityRowView* GetRowForTesting(int n);

  views::View* tab_activity_container() const {
    return tab_activity_container_;
  }
  views::View* group_activity_container() const {
    return group_activity_container_;
  }

 private:
  // Close this bubble.
  void Close();

  // Containers will always be non-null. Visibility is toggled based on
  // whether rows are added to each container.
  raw_ptr<views::View> tab_activity_container_ = nullptr;
  raw_ptr<views::View> group_activity_container_ = nullptr;

  std::vector<ActivityLogItem> activity_log_;
  std::optional<int> current_tab_activity_index_;
  const raw_ptr<Profile> profile_;

  base::WeakPtrFactory<RecentActivityBubbleDialogView> weak_factory_{this};
};

// View containing a single ActivityLogItem. Each row shows activity
// text, metadata text, and an avatar/favicon view.
class RecentActivityRowView : public views::View {
  METADATA_HEADER(RecentActivityRowView, View)

 public:
  RecentActivityRowView(ActivityLogItem item,
                        bool is_current_tab,
                        Profile* profile,
                        base::OnceCallback<void()> close_callback);
  ~RecentActivityRowView() override;

  // views::Views
  bool OnMousePressed(const ui::MouseEvent& event) override;

  RecentActivityRowImageView* image_view() const { return image_view_; }
  const std::u16string& activity_text() const { return activity_text_; }
  const std::u16string& metadata_text() const { return metadata_text_; }

  // RecentActivityAction handlers.
  // Focuses the open tab in the tab strip.
  void FocusTab();
  // Reopens the tab at the end of the group.
  void ReopenTab();
  // Opens the Tab Group editor bubble for the group.
  void OpenTabGroupEditDialog();
  // Opens the Data Sharing management bubble for the group.
  void ManageSharing();

 private:
  std::u16string activity_text_;
  std::u16string metadata_text_;
  raw_ptr<RecentActivityRowImageView> image_view_ = nullptr;
  ActivityLogItem item_;
  const raw_ptr<Profile> profile_ = nullptr;
  base::OnceCallback<void()> close_callback_;
};

// View containing the avatar image and, if the event refers to a tab, the
// favicon of the tab. This view performs both asynchronous image fetches.
class RecentActivityRowImageView : public views::View {
  METADATA_HEADER(RecentActivityRowImageView, View)

 public:
  RecentActivityRowImageView(ActivityLogItem item, Profile* profile);
  ~RecentActivityRowImageView() override;

  // Returns whether there is an avatar image to show.
  bool ShouldShowAvatar() const { return !avatar_image_.isNull(); }

  // Returns whether there is a favicon image to show.
  bool ShouldShowFavicon() const { return !resized_favicon_image_.isNull(); }

 private:
  // views::View
  void OnPaint(gfx::Canvas* canvas) override;

  // Perform the avatar fetch, calling |SetAvatar| when complete.
  void FetchAvatar();
  void SetAvatar(const gfx::Image& avatar);

  // Perform the favicon fetch, calling |SetFavicon| when complete.
  void FetchFavicon();
  void SetFavicon(const favicon_base::FaviconImageResult& favicon);
  void PaintFavicon(gfx::Canvas* canvas, gfx::Rect avatar_bounds);

  base::CancelableTaskTracker favicon_fetching_task_tracker_;
  gfx::ImageSkia avatar_image_;
  gfx::ImageSkia resized_favicon_image_;
  ActivityLogItem item_;
  const raw_ptr<Profile> profile_ = nullptr;

  base::WeakPtrFactory<RecentActivityRowImageView> weak_factory_{this};
};

// The bubble coordinator for Shared Tab Group Recent Activity.
class RecentActivityBubbleCoordinator : public views::WidgetObserver {
 public:
  RecentActivityBubbleCoordinator();
  ~RecentActivityBubbleCoordinator() override;

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // The RecentActivity dialog is used in multiple places, anchoring to
  // different items. Two public method overloads are supplied here so
  // the correct arrow will be used.
  //
  // Calls ShowCommon with the default arrow.
  void Show(views::View* anchor_view,
            content::WebContents* web_contents,
            std::vector<ActivityLogItem> activity_log,
            Profile* profile);
  // Same as above, but provides a default arrow for anchoring to the
  // page action. The default for location bar bubbles is to have a
  // TOP_RIGHT arrow.
  void ShowForCurrentTab(views::View* anchor_view,
                         content::WebContents* web_contents,
                         std::vector<ActivityLogItem> activity_log,
                         Profile* profile);
  void Hide();

  RecentActivityBubbleDialogView* GetBubble() const;
  bool IsShowing();

 private:
  // Show a bubble containing the given activity log.
  void ShowCommon(std::unique_ptr<RecentActivityBubbleDialogView> bubble);

  views::ViewTracker tracker_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_RECENT_ACTIVITY_BUBBLE_DIALOG_VIEW_H_
