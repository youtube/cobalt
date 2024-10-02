// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_dialog_view.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/vector_icons/vector_icons.h"

namespace {

class ShowAllDownloadsButton : public RichHoverButton {
 public:
  explicit ShowAllDownloadsButton(
      base::RepeatingClosure show_all_downloads_callback)
      : RichHoverButton(
            std::move(show_all_downloads_callback),
            /*main_image_icon=*/ui::ImageModel(),
            l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_FOOTER_LINK),
            /*secondary_text=*/std::u16string(),
            l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_FOOTER_TOOLTIP),
            /*subtitle_text=*/std::u16string(),
            ui::ImageModel::FromVectorIcon(vector_icons::kLaunchIcon,
                                           ui::kColorIconSecondary)) {
    // Override the table layout from RichHoverButton, in order to control the
    // spacing/padding. Code below is copied from rich_hover_button.cc but with
    // padding columns rearranged.
    views::TableLayout* table_layout =
        SetLayoutManager(std::make_unique<views::TableLayout>());
    table_layout
        // Column for |main_image_icon|.
        ->AddColumn(views::LayoutAlignment::kCenter,
                    views::LayoutAlignment::kCenter,
                    views::TableLayout::kFixedSize,
                    views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
        // Column for title.
        .AddColumn(views::LayoutAlignment::kStretch,
                   views::LayoutAlignment::kCenter, 1.0f,
                   views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
        // Column for |secondary_text|.
        .AddColumn(views::LayoutAlignment::kEnd,
                   views::LayoutAlignment::kStretch,
                   views::TableLayout::kFixedSize,
                   views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
        // Column for |action_icon|.
        .AddColumn(views::LayoutAlignment::kCenter,
                   views::LayoutAlignment::kCenter,
                   views::TableLayout::kFixedSize,
                   views::TableLayout::ColumnSize::kFixed, 16, 0)
        .AddPaddingColumn(views::TableLayout::kFixedSize,
                          GetLayoutInsets(DOWNLOAD_ICON).right())
        .AddRows(
            1, views::TableLayout::kFixedSize,
            // Force row to have sufficient height for full line-height of
            // the title.
            views::style::GetLineHeight(views::style::CONTEXT_DIALOG_BODY_TEXT,
                                        views::style::STYLE_PRIMARY));

    // TODO(pkasting): This class should subclass Button, not HoverButton.
    table_layout->SetChildViewIgnoredByLayout(image(), true);
    table_layout->SetChildViewIgnoredByLayout(label(), true);
    table_layout->SetChildViewIgnoredByLayout(ink_drop_container(), true);

    Layout();
  }
};

}  // namespace

void DownloadDialogView::CloseBubble() {
  navigation_handler_->CloseDialog(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void DownloadDialogView::ShowAllDownloads() {
  chrome::ShowDownloads(browser_);
}

void DownloadDialogView::AddHeader() {
  auto* header = AddChildView(std::make_unique<views::FlexLayoutView>());
  header->SetOrientation(views::LayoutOrientation::kHorizontal);
  header->SetBorder(views::CreateEmptyBorder(GetLayoutInsets(DOWNLOAD_ROW)));

  auto* title = header->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_HEADER_TEXT),
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY));
  title->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true));
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto* close_button =
      header->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&DownloadDialogView::CloseBubble,
                              base::Unretained(this)),
          vector_icons::kCloseRoundedIcon,
          GetLayoutConstant(DOWNLOAD_ICON_SIZE)));
  InstallCircleHighlightPathGenerator(close_button);
  close_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
  close_button->SetProperty(views::kCrossAxisAlignmentKey,
                            views::LayoutAlignment::kStart);
}

void DownloadDialogView::AddFooter() {
  AddChildView(
      std::make_unique<ShowAllDownloadsButton>(base::BindRepeating(
          &DownloadDialogView::ShowAllDownloads, base::Unretained(this))))
      ->SetBorder(views::CreateEmptyBorder(GetLayoutInsets(DOWNLOAD_ROW)));
}

DownloadDialogView::DownloadDialogView(
    Browser* browser,
    std::unique_ptr<views::View> row_list_scroll_view,
    DownloadBubbleNavigationHandler* navigation_handler)
    : navigation_handler_(navigation_handler), browser_(browser) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  AddHeader();
  AddChildView(std::move(row_list_scroll_view));
  AddFooter();
}

BEGIN_METADATA(DownloadDialogView, views::View)
END_METADATA
