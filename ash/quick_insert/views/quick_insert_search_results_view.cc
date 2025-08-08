// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_search_results_view.h"

#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "ash/ash_element_identifiers.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/quick_insert/model/quick_insert_search_results_section.h"
#include "ash/quick_insert/quick_insert_asset_fetcher.h"
#include "ash/quick_insert/views/quick_insert_emoji_item_view.h"
#include "ash/quick_insert/views/quick_insert_image_item_view.h"
#include "ash/quick_insert/views/quick_insert_item_view.h"
#include "ash/quick_insert/views/quick_insert_list_item_view.h"
#include "ash/quick_insert/views/quick_insert_pseudo_focus.h"
#include "ash/quick_insert/views/quick_insert_search_results_view_delegate.h"
#include "ash/quick_insert/views/quick_insert_section_list_view.h"
#include "ash/quick_insert/views/quick_insert_section_view.h"
#include "ash/quick_insert/views/quick_insert_skeleton_loader_view.h"
#include "ash/quick_insert/views/quick_insert_strings.h"
#include "ash/quick_insert/views/quick_insert_traversable_item_container.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

constexpr int kThrobberDiameter = 32;

constexpr gfx::Insets kNoResultsViewInsets(24);
constexpr int kNoResultsIllustrationAndDescriptionSpacing = 16;
constexpr gfx::Size kNoResultsIllustrationSize(200, 100);

constexpr int kMaxIndexForMetrics = 10;

std::u16string GetAccessibleNameForSeeMoreButton(
    QuickInsertSectionType section_type) {
  switch (section_type) {
    case QuickInsertSectionType::kLinks:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_SEE_MORE_LINKS_BUTTON_ACCESSIBLE_NAME);
    case QuickInsertSectionType::kLocalFiles:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_SEE_MORE_LOCAL_FILES_BUTTON_ACCESSIBLE_NAME);
    case QuickInsertSectionType::kDriveFiles:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_SEE_MORE_DRIVE_FILES_BUTTON_ACCESSIBLE_NAME);
    case QuickInsertSectionType::kNone:
    case QuickInsertSectionType::kClipboard:
    case QuickInsertSectionType::kExamples:
    case QuickInsertSectionType::kContentEditor:
      return u"";
  }
}

QuickInsertSectionView::LocalFileResultStyle ConvertLocalFileResultStyle(
    QuickInsertSearchResultsView::LocalFileResultStyle style) {
  switch (style) {
    case QuickInsertSearchResultsView::LocalFileResultStyle::kList:
      return QuickInsertSectionView::LocalFileResultStyle::kList;
    case QuickInsertSearchResultsView::LocalFileResultStyle::kGrid:
      return QuickInsertSectionView::LocalFileResultStyle::kGrid;
  }
}

}  // namespace

QuickInsertSearchResultsView::QuickInsertSearchResultsView(
    QuickInsertSearchResultsViewDelegate* delegate,
    int quick_insert_view_width,
    PickerAssetFetcher* asset_fetcher,
    PickerSubmenuController* submenu_controller,
    PickerPreviewBubbleController* preview_controller)
    : delegate_(delegate), preview_controller_(preview_controller) {
  SetLayoutManager(std::make_unique<views::BoxLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  SetProperty(views::kElementIdentifierKey,
              kQuickInsertSearchResultsPageElementId);
  GetViewAccessibility().SetRole(ax::mojom::Role::kStatus);
  GetViewAccessibility().SetContainerLiveStatus("polite");

  section_list_view_ = AddChildView(std::make_unique<PickerSectionListView>(
      quick_insert_view_width, asset_fetcher, submenu_controller));
  no_results_view_ = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetVisible(false)
          .SetOrientation(views::LayoutOrientation::kVertical)
          .SetInsideBorderInsets(kNoResultsViewInsets)
          .SetMainAxisAlignment(views::LayoutAlignment::kStart)
          .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
          .SetBetweenChildSpacing(kNoResultsIllustrationAndDescriptionSpacing)
          .AddChildren(
              views::Builder<views::ImageView>()
                  .CopyAddressTo(&no_results_illustration_)
                  .SetVisible(false)
                  .SetImageSize(kNoResultsIllustrationSize),
              views::Builder<views::Label>(
                  bubble_utils::CreateLabel(
                      TypographyToken::kCrosBody2,
                      l10n_util::GetStringUTF16(IDS_PICKER_NO_RESULTS_TEXT),
                      cros_tokens::kCrosSysOnSurfaceVariant))
                  .CopyAddressTo(&no_results_label_)
                  .SetHorizontalAlignment(gfx::ALIGN_CENTER))
          .Build());

  skeleton_loader_view_ = AddChildView(
      views::Builder<PickerSkeletonLoaderView>().SetVisible(false).Build());

  throbber_container_ = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::LayoutOrientation::kVertical)
          .SetInsideBorderInsets(kNoResultsViewInsets)
          .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
          .AddChildren(
              views::Builder<views::SmoothedThrobber>(
                  std::make_unique<views::SmoothedThrobber>(kThrobberDiameter))
                  .CopyAddressTo(&throbber_)
                  .SetStartDelay(kLoadingAnimationDelay))
          .Build());
}

QuickInsertSearchResultsView::~QuickInsertSearchResultsView() = default;

void QuickInsertSearchResultsView::SetLocalFileResultStyle(
    LocalFileResultStyle style) {
  local_file_result_style_ = style;
}

views::View* QuickInsertSearchResultsView::GetTopItem() {
  return section_list_view_->GetTopItem();
}

views::View* QuickInsertSearchResultsView::GetBottomItem() {
  return section_list_view_->GetBottomItem();
}

views::View* QuickInsertSearchResultsView::GetItemAbove(views::View* item) {
  if (!Contains(item)) {
    return nullptr;
  }
  if (views::IsViewClass<QuickInsertItemView>(item)) {
    // Skip views that aren't QuickInsertItemViews, to allow users to quickly
    // navigate between items.
    return section_list_view_->GetItemAbove(item);
  }
  views::View* prev_item = GetNextPickerPseudoFocusableView(
      item, PickerPseudoFocusDirection::kBackward, /*should_loop=*/false);
  return Contains(prev_item) ? prev_item : nullptr;
}

views::View* QuickInsertSearchResultsView::GetItemBelow(views::View* item) {
  if (!Contains(item)) {
    return nullptr;
  }
  if (views::IsViewClass<QuickInsertItemView>(item)) {
    // Skip views that aren't QuickInsertItemViews, to allow users to quickly
    // navigate between items.
    return section_list_view_->GetItemBelow(item);
  }
  views::View* next_item = GetNextPickerPseudoFocusableView(
      item, PickerPseudoFocusDirection::kForward, /*should_loop=*/false);
  return Contains(next_item) ? next_item : nullptr;
}

views::View* QuickInsertSearchResultsView::GetItemLeftOf(views::View* item) {
  if (!Contains(item)) {
    return nullptr;
  }
  return section_list_view_->GetItemLeftOf(item);
}

views::View* QuickInsertSearchResultsView::GetItemRightOf(views::View* item) {
  if (!Contains(item)) {
    return nullptr;
  }
  return section_list_view_->GetItemRightOf(item);
}

bool QuickInsertSearchResultsView::ContainsItem(views::View* item) {
  return Contains(item);
}

void QuickInsertSearchResultsView::ClearSearchResults() {
  section_views_.clear();
  section_list_view_->ClearSectionList();
  section_list_view_->SetVisible(true);
  no_results_view_->SetVisible(false);
  StopLoadingAnimation();
  StartThrobber();
  top_results_.clear();
  delegate_->OnSearchResultsViewHeightChanged();
  UpdateAccessibleName();
}

void QuickInsertSearchResultsView::AppendSearchResults(
    QuickInsertSearchResultsSection section) {
  StopLoadingAnimation();
  StopThrobber();

  auto* section_view = section_list_view_->AddSection();
  std::u16string section_title =
      GetSectionTitleForQuickInsertSectionType(section.type());
  section_view->AddTitleLabel(section_title);
  if (section.has_more_results()) {
    section_view->AddTitleTrailingLink(
        l10n_util::GetStringUTF16(IDS_PICKER_SEE_MORE_BUTTON_TEXT),
        GetAccessibleNameForSeeMoreButton(section.type()),
        base::BindRepeating(
            &QuickInsertSearchResultsView::OnTrailingLinkClicked,
            base::Unretained(this), section.type()));
  }
  for (const auto& result : section.results()) {
    AddResultToSection(result, section_view);
    if (top_results_.size() < kMaxIndexForMetrics) {
      top_results_.push_back(result);
    }
  }
  section_views_.push_back(section_view);

  delegate_->RequestPseudoFocus(section_list_view_->GetTopItem());
  delegate_->OnSearchResultsViewHeightChanged();
  UpdateAccessibleName();
}

bool QuickInsertSearchResultsView::SearchStopped(ui::ImageModel illustration,
                                                 std::u16string description) {
  StopLoadingAnimation();
  StopThrobber();
  if (!section_views_.empty()) {
    return false;
  }
  no_results_illustration_->SetVisible(!illustration.IsEmpty());
  no_results_illustration_->SetImage(std::move(illustration));
  no_results_label_->SetText(std::move(description));
  no_results_view_->SetVisible(true);
  section_list_view_->SetVisible(false);
  delegate_->OnSearchResultsViewHeightChanged();
  UpdateAccessibleName();
  return true;
}

void QuickInsertSearchResultsView::ShowLoadingAnimation() {
  ClearSearchResults();
  StopThrobber();
  skeleton_loader_view_->StartAnimationAfter(kLoadingAnimationDelay);
  skeleton_loader_view_->SetVisible(true);
  delegate_->OnSearchResultsViewHeightChanged();
}

void QuickInsertSearchResultsView::SelectSearchResult(
    const QuickInsertSearchResult& result) {
  delegate_->SelectSearchResult(result);
}

void QuickInsertSearchResultsView::AddResultToSection(
    const QuickInsertSearchResult& result,
    QuickInsertSectionView* section_view) {
  // `base::Unretained` is safe here because `this` will own the item view which
  // takes this callback.
  QuickInsertItemView* view = section_view->AddResult(
      result, preview_controller_,
      ConvertLocalFileResultStyle(local_file_result_style_),
      base::BindRepeating(&QuickInsertSearchResultsView::SelectSearchResult,
                          base::Unretained(this), result));

  if (auto* list_item_view =
          views::AsViewClass<QuickInsertListItemView>(view)) {
    list_item_view->SetBadgeAction(delegate_->GetActionForResult(result));
  } else if (auto* image_item_view =
                 views::AsViewClass<PickerImageItemView>(view)) {
    image_item_view->SetAction(delegate_->GetActionForResult(result));
  }
}

void QuickInsertSearchResultsView::OnTrailingLinkClicked(
    QuickInsertSectionType section_type,
    const ui::Event& event) {
  delegate_->SelectMoreResults(section_type);
}

int QuickInsertSearchResultsView::GetIndex(
    const QuickInsertSearchResult& inserted_result) {
  if (top_results_.empty()) {
    return -1;
  }
  auto it = base::ranges::find(top_results_, inserted_result);
  if (it == top_results_.end()) {
    return kMaxIndexForMetrics;
  }
  return std::min(kMaxIndexForMetrics,
                  static_cast<int>(it - top_results_.begin()));
}

void QuickInsertSearchResultsView::SetNumEmojiResultsForA11y(
    size_t num_emoji_results) {
  num_emoji_results_displayed_ = num_emoji_results;
}

void QuickInsertSearchResultsView::StartThrobber() {
  throbber_container_->SetVisible(true);
  throbber_->Start();
  delegate_->OnSearchResultsViewHeightChanged();
}

void QuickInsertSearchResultsView::StopThrobber() {
  throbber_container_->SetVisible(false);
  throbber_->Stop();
  delegate_->OnSearchResultsViewHeightChanged();
}

void QuickInsertSearchResultsView::StopLoadingAnimation() {
  skeleton_loader_view_->StopAnimation();
  skeleton_loader_view_->SetVisible(false);
  delegate_->OnSearchResultsViewHeightChanged();
}

void QuickInsertSearchResultsView::UpdateAccessibleName() {
  // If the sections are empty but the no results view is not visible, it means
  // we are in a pending state, which should not have an announcement.
  if (!section_views_.empty() || !no_results_view_->GetVisible()) {
    GetViewAccessibility().SetName(u"");
    return;
  }

  // Avoid announcing the same "no results found" live region consecutively.
  const std::u16string accessible_name =
      num_emoji_results_displayed_ == 0
          ? l10n_util::GetStringUTF16(IDS_PICKER_NO_RESULTS_TEXT)
          : l10n_util::GetPluralStringFUTF16(
                IDS_PICKER_EMOJI_SEARCH_RESULTS_ACCESSIBILITY_ANNOUNCEMENT_TEXT,
                num_emoji_results_displayed_);
  if (GetAccessibleName() == accessible_name) {
    return;
  }
  GetViewAccessibility().SetName(std::move(accessible_name));
  NotifyAccessibilityEvent(ax::mojom::Event::kLiveRegionChanged, true);
}

BEGIN_METADATA(QuickInsertSearchResultsView)
END_METADATA

}  // namespace ash
