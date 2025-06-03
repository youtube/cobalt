// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_style.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {

constexpr int kHeaderChipVerticalInset = 1;
constexpr int kTitleAdjustmentForEmptyHeader = 2;
constexpr int kTitleAdjustmentForNonEmptyHeader = -2;
// The default size of an empty chip in the tab group header.
constexpr int kEmptyChipSize = 14;
// The width of the sync icon when a tab group is saved.
constexpr int kSyncIconWidth = 16;
// The size of the empty chips when the #tab-groups-save flag is on.
constexpr int kSavedEmptyChipSize = 22;

constexpr int kChromeRefreshHeaderChipVerticalInset = 2;
constexpr int kChromeRefreshEmptyChipSize = 20;
constexpr int kChromeRefreshSyncIconWidth = 16;
constexpr int kChromeRefreshSyncIconLeftMargin = 2;
constexpr int kChromeRefreshCornerRadius = 6;
constexpr int kTabGroupOverlapAdjustment = 2;

}  // namespace

TabGroupStyle::TabGroupStyle(const TabGroupViews& tab_group_views)
    : tab_group_views_(tab_group_views) {}

TabGroupStyle::~TabGroupStyle() = default;

bool TabGroupStyle::TabGroupUnderlineShouldBeHidden() const {
  return false;
}

bool TabGroupStyle::TabGroupUnderlineShouldBeHidden(
    const views::View* const leading_view,
    const views::View* const trailing_view) const {
  return false;
}

// The underline is a straight line with half-rounded endcaps without
// ChromeRefresh flag. Since this geometry is nontrivial to represent using
// primitives, it's instead represented using a fill path.
SkPath TabGroupStyle::GetUnderlinePath(const gfx::Rect local_bounds) const {
  SkPath path;

  path.moveTo(0, TabGroupUnderline::kStrokeThickness);
  path.arcTo(TabGroupUnderline::kStrokeThickness,
             TabGroupUnderline::kStrokeThickness, 0, SkPath::kSmall_ArcSize,
             SkPathDirection::kCW, TabGroupUnderline::kStrokeThickness, 0);
  path.lineTo(local_bounds.width() - TabGroupUnderline::kStrokeThickness, 0);
  path.arcTo(TabGroupUnderline::kStrokeThickness,
             TabGroupUnderline::kStrokeThickness, 0, SkPath::kSmall_ArcSize,
             SkPathDirection::kCW, local_bounds.width(),
             TabGroupUnderline::kStrokeThickness);
  path.close();

  return path;
}

gfx::Rect TabGroupStyle::GetEmptyTitleChipBounds(
    const TabGroupHeader* const header) const {
  return gfx::Rect(GetTitleChipOffset(absl::nullopt).x(),
                   GetTitleChipOffset(absl::nullopt).y(), GetEmptyChipSize(),
                   GetEmptyChipSize());
}

gfx::Point TabGroupStyle::GetTitleChipOffset(
    absl::optional<int> text_height) const {
  if (text_height.has_value()) {
    return gfx::Point(
        TabGroupUnderline::GetStrokeInset(),
        ((GetLayoutConstant(TAB_HEIGHT) - text_height.value()) / 2 -
         GetInsetsForHeaderChip(false).top()));
  } else {
    return gfx::Point(TabGroupUnderline::GetStrokeInset(),
                      (GetLayoutConstant(TAB_HEIGHT) - GetEmptyChipSize()) / 2);
  }
}

std::unique_ptr<views::Background> TabGroupStyle::GetEmptyTitleChipBackground(
    const SkColor color) const {
  return views::CreateRoundedRectBackground(color, GetEmptyChipSize() / 2);
}

gfx::Insets TabGroupStyle::GetInsetsForHeaderChip(
    bool should_show_sync_icon) const {
  return gfx::Insets::TLBR(kHeaderChipVerticalInset,
                           GetChipCornerRadius() + kHeaderChipVerticalInset,
                           kHeaderChipVerticalInset,
                           GetChipCornerRadius() + kHeaderChipVerticalInset);
}

int TabGroupStyle::GetHighlightPathGeneratorCornerRadius(
    const views::View* const title) const {
  return title->GetVisible() ? GetChipCornerRadius() : GetEmptyChipSize() / 2;
}

int TabGroupStyle::GetTitleAdjustmentToTabGroupHeaderDesiredWidth(
    const std::u16string title) const {
  return title.empty() ? kTitleAdjustmentForEmptyHeader
                       : kTitleAdjustmentForNonEmptyHeader;
}

float TabGroupStyle::GetEmptyChipSize() const {
  return base::FeatureList::IsEnabled(features::kTabGroupsSave)
             ? kSavedEmptyChipSize
             : kEmptyChipSize;
}

float TabGroupStyle::GetSyncIconWidth() const {
  return kSyncIconWidth;
}

int TabGroupStyle::GetChipCornerRadius() const {
  return TabStyle::Get()->GetBottomCornerRadius() -
         TabGroupUnderline::kStrokeThickness;
}

int TabGroupStyle::GetTabGroupViewOverlap() const {
  return TabStyle::Get()->GetTabOverlap();
}

ChromeRefresh2023TabGroupStyle::ChromeRefresh2023TabGroupStyle(
    const TabGroupViews& tab_group_views)
    : TabGroupStyle(tab_group_views) {}

ChromeRefresh2023TabGroupStyle::~ChromeRefresh2023TabGroupStyle() = default;

bool ChromeRefresh2023TabGroupStyle::TabGroupUnderlineShouldBeHidden() const {
  const auto [leading_group_view, trailing_group_view] =
      tab_group_views_->GetLeadingTrailingGroupViews();

  return TabGroupUnderlineShouldBeHidden(leading_group_view,
                                         trailing_group_view);
}

bool ChromeRefresh2023TabGroupStyle::TabGroupUnderlineShouldBeHidden(
    const views::View* const leading_view,
    const views::View* const trailing_view) const {
  const TabGroupHeader* const leading_view_group_header =
      views::AsViewClass<TabGroupHeader>(leading_view);
  const TabGroupHeader* const trailing_view_group_header =
      views::AsViewClass<TabGroupHeader>(trailing_view);

  if (leading_view_group_header && trailing_view_group_header &&
      leading_view_group_header == trailing_view_group_header) {
    return true;
  }

  return false;
}

// The path is a rounded rect with the Chrome Refresh flag.
SkPath ChromeRefresh2023TabGroupStyle::GetUnderlinePath(
    const gfx::Rect local_bounds) const {
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(local_bounds),
                    TabGroupUnderline::kStrokeThickness / 2,
                    TabGroupUnderline::kStrokeThickness / 2);
  return path;
}

gfx::Rect ChromeRefresh2023TabGroupStyle::GetEmptyTitleChipBounds(
    const TabGroupHeader* const header) const {
  return gfx::Rect(GetTitleChipOffset(absl::nullopt).x(),
                   GetTitleChipOffset(absl::nullopt).y(), GetEmptyChipSize(),
                   GetEmptyChipSize());
}

gfx::Point ChromeRefresh2023TabGroupStyle::GetTitleChipOffset(
    absl::optional<int> text_height) const {
  const int total_space = GetLayoutConstant(TAB_STRIP_HEIGHT) -
                          GetEmptyChipSize() -
                          GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP);
  return gfx::Point(TabStyle::Get()->GetTabOverlap() - 2, total_space / 2);
}

gfx::Insets ChromeRefresh2023TabGroupStyle::GetInsetsForHeaderChip(
    bool should_show_sync_icon) const {
  return gfx::Insets::TLBR(
      kChromeRefreshHeaderChipVerticalInset,
      should_show_sync_icon ? kChromeRefreshSyncIconLeftMargin
                            : GetChipCornerRadius(),
      kChromeRefreshHeaderChipVerticalInset, GetChipCornerRadius());
}

int ChromeRefresh2023TabGroupStyle::
    GetTitleAdjustmentToTabGroupHeaderDesiredWidth(
        const std::u16string title) const {
  // Since the shape of the header in ChromeRefresh23 is a rounded rect this
  // value should be `kTitleAdjustmentForNonEmptyHeader`.
  return kTitleAdjustmentForNonEmptyHeader;
}

std::unique_ptr<views::Background>
ChromeRefresh2023TabGroupStyle::GetEmptyTitleChipBackground(
    const SkColor color) const {
  return views::CreateRoundedRectBackground(color, GetChipCornerRadius());
}

int ChromeRefresh2023TabGroupStyle::GetHighlightPathGeneratorCornerRadius(
    const views::View* const title) const {
  return GetChipCornerRadius();
}

float ChromeRefresh2023TabGroupStyle::GetEmptyChipSize() const {
  return kChromeRefreshEmptyChipSize;
}

float ChromeRefresh2023TabGroupStyle::GetSyncIconWidth() const {
  return kChromeRefreshSyncIconWidth;
}

int ChromeRefresh2023TabGroupStyle::GetChipCornerRadius() const {
  return kChromeRefreshCornerRadius;
}

int ChromeRefresh2023TabGroupStyle::GetTabGroupViewOverlap() const {
  // For refresh the tab has an overlap value is 18. Inorder to have a margin of
  // 10 from the neighbor tabs this is required.
  return TabStyle::Get()->GetTabOverlap() - GetTabGroupOverlapAdjustment();
}

// static
int ChromeRefresh2023TabGroupStyle::GetTabGroupOverlapAdjustment() {
  return kTabGroupOverlapAdjustment;
}
