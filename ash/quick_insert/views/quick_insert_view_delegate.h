// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_VIEW_DELEGATE_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_VIEW_DELEGATE_H_

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "ash/quick_insert/model/quick_insert_mode_type.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ui/base/emoji/emoji_panel_helper.h"

namespace ash {

enum class QuickInsertActionType;
enum class QuickInsertCapsLockPosition;
class QuickInsertAssetFetcher;
class QuickInsertSearchResultsSection;
class QuickInsertSessionMetrics;

// Delegate for `QuickInsertView`.
class ASH_EXPORT QuickInsertViewDelegate {
 public:
  using SearchResultsCallback = base::RepeatingCallback<void(
      std::vector<QuickInsertSearchResultsSection> results)>;
  using EmojiSearchResultsCallback =
      base::OnceCallback<void(std::vector<QuickInsertEmojiResult> results)>;
  using SuggestedEditorResultsCallback =
      base::OnceCallback<void(std::vector<QuickInsertSearchResult> results)>;
  using SuggestedResultsCallback = base::RepeatingCallback<void(
      std::vector<QuickInsertSearchResult> results)>;

  virtual ~QuickInsertViewDelegate() {}

  virtual std::vector<QuickInsertCategory> GetAvailableCategories() = 0;

  // Gets suggested results for the zero-state. Results will be returned via
  // `callback`, which may be called multiples times to update the results.
  virtual void GetZeroStateSuggestedResults(
      SuggestedResultsCallback callback) = 0;

  // Gets initially suggested results for category. Results will be returned via
  // `callback`, which may be called multiples times to update the results.
  virtual void GetResultsForCategory(QuickInsertCategory category,
                                     SearchResultsCallback callback) = 0;

  // Starts a search for `query`. Results will be returned via `callback`,
  // which may be called multiples times to update the results.
  // If `callback` is called with empty results, then it will never be called
  // again (i.e. all search results have been returned).
  virtual void StartSearch(std::u16string_view query,
                           std::optional<QuickInsertCategory> category,
                           SearchResultsCallback callback) = 0;

  // Stops the previous search, if any.
  virtual void StopSearch() = 0;

  // Starts a emoji search for `query`. Results will be returned via `callback`.
  virtual void StartEmojiSearch(std::u16string_view query,
                                EmojiSearchResultsCallback callback) = 0;

  // Closes the Widget and inserts `result` into the next focused input field.
  // If there's no focus event within some timeout after the widget is closed,
  // the result is dropped silently.
  virtual void CloseWidgetThenInsertResultOnNextFocus(
      const QuickInsertSearchResult& result) = 0;

  // Opens `result`. The exact behavior varies on the type of result.
  virtual void OpenResult(const QuickInsertSearchResult& result) = 0;

  // Shows the Emoji Picker with `category`.
  virtual void ShowEmojiPicker(ui::EmojiPickerCategory category,
                               std::u16string_view query) = 0;

  // Shows the Editor.
  virtual void ShowEditor(std::optional<std::string> preset_query_id,
                          std::optional<std::string> freeform_text) = 0;

  virtual void ShowLobster(std::optional<std::string> freeform_text) = 0;

  // Returns the current action for `result`.
  virtual QuickInsertActionType GetActionForResult(
      const QuickInsertSearchResult& result) = 0;

  virtual QuickInsertAssetFetcher* GetAssetFetcher() = 0;

  virtual QuickInsertSessionMetrics& GetSessionMetrics() = 0;

  // Gets suggested emoji results.
  virtual std::vector<QuickInsertEmojiResult> GetSuggestedEmoji() = 0;

  // Whether GIFs are enabled or not.
  virtual bool IsGifsEnabled() = 0;

  virtual QuickInsertModeType GetMode() = 0;

  virtual QuickInsertCapsLockPosition GetCapsLockPosition() = 0;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_VIEW_DELEGATE_H_
