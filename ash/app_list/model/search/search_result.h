// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_SEARCH_SEARCH_RESULT_H_
#define ASH_APP_LIST_MODEL_SEARCH_SEARCH_RESULT_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/model/app_list_model_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/observer_list.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/range/range.h"

namespace ui {
class ImageModel;
}  // namespace ui

namespace ash {

class SearchResultObserver;

// SearchResult consists of an icon, title text and details text. Title and
// details text can have tagged ranges that are displayed differently from
// default style.
class APP_LIST_MODEL_EXPORT SearchResult {
 public:
  using Category = ash::AppListSearchResultCategory;
  using ResultType = ash::AppListSearchResultType;
  using DisplayType = ash::SearchResultDisplayType;
  using MetricsType = ash::SearchResultType;
  using Tag = ash::SearchResultTag;
  using Tags = ash::SearchResultTags;
  using Action = ash::SearchResultAction;
  using Actions = ash::SearchResultActions;
  using IconInfo = ash::SearchResultIconInfo;
  using IconShape = ash::SearchResultIconShape;
  using TextItem = ash::SearchResultTextItem;
  using TextVector = std::vector<TextItem>;

  SearchResult();
  SearchResult(const SearchResult&) = delete;
  SearchResult& operator=(const SearchResult&) = delete;
  virtual ~SearchResult();

  const IconInfo& icon() const { return metadata_->icon; }
  void SetIcon(const IconInfo& icon);

  const gfx::ImageSkia& chip_icon() const { return metadata_->chip_icon; }
  void SetChipIcon(const gfx::ImageSkia& chip_icon);

  const ui::ImageModel& badge_icon() const { return metadata_->badge_icon; }
  void SetBadgeIcon(const ui::ImageModel& badge_icon);

  const std::u16string& title() const { return metadata_->title; }
  void SetTitle(const std::u16string& title);

  const Tags& title_tags() const { return metadata_->title_tags; }
  void SetTitleTags(const Tags& tags);

  const TextVector& title_text_vector() const {
    return metadata_->title_vector;
  }
  void SetTitleTextVector(const TextVector& vector);

  bool multiline_title() const { return metadata_->multiline_title; }
  void SetMultilineTitle(bool multiline_title);

  const std::u16string& details() const { return metadata_->details; }
  void SetDetails(const std::u16string& details);

  const Tags& details_tags() const { return metadata_->details_tags; }
  void SetDetailsTags(const Tags& tags);

  const TextVector& details_text_vector() const {
    return metadata_->details_vector;
  }
  void SetDetailsTextVector(const TextVector& vector);

  bool multiline_details() const { return metadata_->multiline_details; }
  void SetMultilineDetails(bool multiline_details);

  const TextVector& big_title_text_vector() const {
    return metadata_->big_title_vector;
  }
  void SetBigTitleTextVector(const TextVector& vector);

  const TextVector& big_title_superscript_text_vector() const {
    return metadata_->big_title_superscript_vector;
  }
  void SetBigTitleSuperscriptTextVector(const TextVector& vector);

  const TextVector& keyboard_shortcut_text_vector() const {
    return metadata_->keyboard_shortcut_vector;
  }
  void SetKeyboardShortcutTextVector(const TextVector& vector);

  const std::u16string& accessible_name() const {
    return metadata_->accessible_name;
  }
  void SetAccessibleName(const std::u16string& name);

  float rating() const { return metadata_->rating; }
  void SetRating(float rating);

  const std::u16string& formatted_price() const {
    return metadata_->formatted_price;
  }
  void SetFormattedPrice(const std::u16string& formatted_price);

  const std::string& id() const { return metadata_->id; }

  double display_score() const { return metadata_->display_score; }
  void set_display_score(double display_score) {
    metadata_->display_score = display_score;
  }

  Category category() const { return metadata_->category; }
  void set_category(Category category) { metadata_->category = category; }

  bool best_match() const { return metadata_->best_match; }
  void set_best_match(bool best_match) { metadata_->best_match = best_match; }

  DisplayType display_type() const { return metadata_->display_type; }
  void set_display_type(DisplayType display_type) {
    metadata_->display_type = display_type;
  }

  ResultType result_type() const { return metadata_->result_type; }
  void set_result_type(ResultType result_type) {
    metadata_->result_type = result_type;
  }

  MetricsType metrics_type() const { return metadata_->metrics_type; }
  void set_metrics_type(MetricsType metrics_type) {
    metadata_->metrics_type = metrics_type;
  }

  const Actions& actions() const { return metadata_->actions; }
  void SetActions(const Actions& sets);

  bool is_omnibox_search() const { return metadata_->is_omnibox_search; }
  void set_is_omnibox_search(bool is_omnibox_search) {
    metadata_->is_omnibox_search = is_omnibox_search;
  }

  bool is_visible() const { return is_visible_; }
  void set_is_visible(bool is_visible) { is_visible_ = is_visible; }

  bool is_recommendation() const { return metadata_->is_recommendation; }
  void set_is_recommendation(bool is_recommendation) {
    metadata_->is_recommendation = is_recommendation;
  }

  bool skip_update_animation() const {
    return metadata_->skip_update_animation;
  }
  void set_skip_update_animation(bool skip_update_animation) {
    metadata_->skip_update_animation = skip_update_animation;
  }

  bool use_badge_icon_background() const {
    return metadata_->use_badge_icon_background;
  }

  void AddObserver(SearchResultObserver* observer);
  void RemoveObserver(SearchResultObserver* observer);

  // Invokes a custom action on the result. It does nothing by default.
  virtual void InvokeAction(int action_index, int event_flags);

  void SetMetadata(std::unique_ptr<SearchResultMetadata> metadata);
  std::unique_ptr<SearchResultMetadata> TakeMetadata() {
    return std::move(metadata_);
  }
  std::unique_ptr<SearchResultMetadata> CloneMetadata() const {
    return std::make_unique<SearchResultMetadata>(*metadata_);
  }

 protected:
  void set_id(const std::string& id) { metadata_->id = id; }

 private:
  friend class SearchController;
  // TODO(crbug.com/1352636) Remove this friend class. Currently used to mock
  // results for SearchResultImageView prototyping.
  friend class SearchResultImageView;

  // Opens the result. Clients should use AppListViewDelegate::OpenSearchResult.
  virtual void Open(int event_flags);

  bool is_visible_ = true;

  std::unique_ptr<SearchResultMetadata> metadata_;

  base::ObserverList<SearchResultObserver> observers_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_SEARCH_SEARCH_RESULT_H_
