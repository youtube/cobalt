// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_EMOJI_BAR_VIEW_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_EMOJI_BAR_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/views/quick_insert_traversable_item_container.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Button;
}

namespace ash {

class IconButton;
class QuickInsertEmojiBarViewDelegate;
class SystemShadow;

// View for the Quick Insert emoji bar, which is a small bar above the main
// Quick Insert container that shows expression search results (i.e. emojis,
// symbols and emoticons).
class ASH_EXPORT QuickInsertEmojiBarView
    : public views::View,
      public QuickInsertTraversableItemContainer {
  METADATA_HEADER(QuickInsertEmojiBarView, views::View)

 public:
  // `delegate` must remain valid for the lifetime of this class.
  QuickInsertEmojiBarView(QuickInsertEmojiBarViewDelegate* delegate,
                          int quick_insert_view_width,
                          bool is_gifs_enabled = false);
  QuickInsertEmojiBarView(const QuickInsertEmojiBarView&) = delete;
  QuickInsertEmojiBarView& operator=(const QuickInsertEmojiBarView&) = delete;
  ~QuickInsertEmojiBarView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // QuickInsertTraversableItemContainer:
  views::View* GetTopItem() override;
  views::View* GetBottomItem() override;
  views::View* GetItemAbove(views::View* item) override;
  views::View* GetItemBelow(views::View* item) override;
  views::View* GetItemLeftOf(views::View* item) override;
  views::View* GetItemRightOf(views::View* item) override;
  bool ContainsItem(views::View* item) override;

  // Clears the emoji bar's search results.
  void ClearSearchResults();

  // Sets the results from `section` as the emoji bar's search results.
  void SetSearchResults(std::vector<QuickInsertEmojiResult> results);

  // Returns the number of items displayed in the emoji bar.
  size_t GetNumItems() const;

  views::View::Views GetItemsForTesting() const;

  views::Button* gifs_button_for_testing() { return gifs_button_; }

  IconButton* more_emojis_button_for_testing() { return more_emojis_button_; }

 private:
  void SelectSearchResult(const QuickInsertSearchResult& result);

  void OpenMoreEmojis();

  void ToggleGifs(bool is_checked);

  int CalculateAvailableWidthForItemRow();

  // Returns the leftmost child view in the emoji bar, or nullptr if there is no
  // such view.
  views::View* GetLeftmostItem();

  std::unique_ptr<SystemShadow> shadow_;

  // `delegate_` outlives `this`.
  raw_ptr<QuickInsertEmojiBarViewDelegate> delegate_;

  // The width of the QuickInsertView that contains this emoji bar.
  int quick_insert_view_width_ = 0;

  // Contains the item views corresponding to each search result.
  raw_ptr<views::View> item_row_ = nullptr;

  // The button for opening the gif picker.
  raw_ptr<views::Button> gifs_button_ = nullptr;

  // The button for opening more emojis.
  raw_ptr<IconButton> more_emojis_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_EMOJI_BAR_VIEW_H_
