// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_SEARCH_RESULTS_VIEW_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_SEARCH_RESULTS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/quick_insert/model/quick_insert_search_results_section.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/views/quick_insert_page_view.h"
#include "ash/quick_insert/views/quick_insert_submenu_controller.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"

namespace views {
class ImageView;
class Label;
class Throbber;
class View;
}  // namespace views

namespace ash {

class QuickInsertAssetFetcher;
class QuickInsertSearchResultsViewDelegate;
class QuickInsertSectionListView;
class QuickInsertSectionView;
class QuickInsertPreviewBubbleController;
class QuickInsertSkeletonLoaderView;

class ASH_EXPORT QuickInsertSearchResultsView : public QuickInsertPageView {
  METADATA_HEADER(QuickInsertSearchResultsView, QuickInsertPageView)

 public:
  // Describes the way local file results are visually presented.
  enum class LocalFileResultStyle {
    // Shown as list items with the name of the file as the label.
    kList,
    // Shown as a grid of thumbnail previews.
    kGrid,
  };

  // `delegate`, `asset_fetcher`, `submenu_controller`, `preview_controller`
  // must remain valid for the lifetime of this class.
  explicit QuickInsertSearchResultsView(
      QuickInsertSearchResultsViewDelegate* delegate,
      int quick_insert_view_width,
      QuickInsertAssetFetcher* asset_fetcher,
      QuickInsertSubmenuController* submenu_controller,
      QuickInsertPreviewBubbleController* preview_controller);
  QuickInsertSearchResultsView(const QuickInsertSearchResultsView&) = delete;
  QuickInsertSearchResultsView& operator=(const QuickInsertSearchResultsView&) =
      delete;
  ~QuickInsertSearchResultsView() override;

  // The skeleton loader should not be used for short loading times.
  // Wait for a delay before showing the animation.
  static constexpr base::TimeDelta kLoadingAnimationDelay =
      base::Milliseconds(400);

  void SetLocalFileResultStyle(LocalFileResultStyle style);

  // QuickInsertPageView:
  views::View* GetTopItem() override;
  views::View* GetBottomItem() override;
  views::View* GetItemAbove(views::View* item) override;
  views::View* GetItemBelow(views::View* item) override;
  views::View* GetItemLeftOf(views::View* item) override;
  views::View* GetItemRightOf(views::View* item) override;
  bool ContainsItem(views::View* item) override;

  // Clears the search results.
  void ClearSearchResults();

  // Append `section` to the current set of search results.
  // TODO: b/325840864 - Merge with existing sections if needed.
  void AppendSearchResults(QuickInsertSearchResultsSection section);

  // Marks that no more search results will be appended until a
  // `ClearSearchResults()` call.
  // Returns whether the "no more results" screen was shown.
  // `illustration` is shown in the center, with `description` shown below it.
  // If `illustration` is empty, then only the description is shown.
  bool SearchStopped(ui::ImageModel illustration, std::u16string description);

  void ShowLoadingAnimation();

  // Returns the index of `inserted_result` in the search result list.
  int GetIndex(const QuickInsertSearchResult& inserted_result);

  // Sets the number of emoji results for accessibility.
  void SetNumEmojiResultsForA11y(size_t num_emoji_results);

  QuickInsertSectionListView* section_list_view_for_testing() {
    return section_list_view_;
  }

  base::span<const raw_ptr<QuickInsertSectionView>> section_views_for_testing()
      const {
    return section_views_;
  }

  views::View* no_results_view_for_testing() { return no_results_view_; }
  views::ImageView& no_results_illustration_for_testing() {
    return *no_results_illustration_;
  }
  views::Label& no_results_label_for_testing() { return *no_results_label_; }

  views::View& throbber_container_for_testing() { return *throbber_container_; }

  QuickInsertSkeletonLoaderView& skeleton_loader_view_for_testing() {
    return *skeleton_loader_view_;
  }

 private:
  // Runs `select_search_result_callback_` on `result`. Note that only one
  // result can be selected (and subsequently calling this method will do
  // nothing).
  void SelectSearchResult(const QuickInsertSearchResult& result);

  // Adds a result item view to `section_view` based on what type `result` is.
  void AddResultToSection(const QuickInsertSearchResult& result,
                          QuickInsertSectionView* section_view);

  void OnTrailingLinkClicked(QuickInsertSectionType section_type,
                             const ui::Event& event);

  void StartThrobber();
  void StopThrobber();

  void StopLoadingAnimation();
  void UpdateAccessibleName();

  raw_ptr<QuickInsertSearchResultsViewDelegate> delegate_;

  // The section list view, contains the section views.
  raw_ptr<QuickInsertSectionListView> section_list_view_ = nullptr;

  // Used to track the views for each section of results.
  std::vector<raw_ptr<QuickInsertSectionView>> section_views_;

  // Used to calculate the index of the inserted result.
  std::vector<QuickInsertSearchResult> top_results_;

  // A view for when there are no results.
  raw_ptr<views::View> no_results_view_ = nullptr;
  raw_ptr<views::ImageView> no_results_illustration_ = nullptr;
  raw_ptr<views::Label> no_results_label_ = nullptr;

  // The throbber is shown when results are pending and the skeleton loader is
  // not already being shown.
  raw_ptr<views::View> throbber_container_ = nullptr;
  raw_ptr<views::Throbber> throbber_ = nullptr;

  // The skeleton loader can be shown when results are pending.
  raw_ptr<QuickInsertSkeletonLoaderView> skeleton_loader_view_ = nullptr;

  raw_ptr<QuickInsertPreviewBubbleController> preview_controller_;

  // Number of emoji search results displayed by the emoji bar. Used for
  // accessibility announcements.
  int num_emoji_results_displayed_ = 0;

  LocalFileResultStyle local_file_result_style_ = LocalFileResultStyle::kList;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_SEARCH_RESULTS_VIEW_H_
