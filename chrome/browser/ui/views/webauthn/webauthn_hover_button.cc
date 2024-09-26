// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/webauthn_hover_button.h"

#include "base/strings/string_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {

// IconWrapper wraps an icon for placing it into a
// |WebAuthnHoverButton|. It is a near copy of the base HoverButton's
// internal class of the same name, but the horizontal spacing applied
// by that class is incompatible with the WebAuthn UI spec.
class IconWrapper : public views::View {
 public:
  METADATA_HEADER(IconWrapper);
  explicit IconWrapper(std::unique_ptr<views::View> icon) {
    AddChildView(std::move(icon));
    SetUseDefaultFillLayout(true);
    // Make sure hovering over the icon also hovers the |HoverButton|.
    SetCanProcessEventsWithinSubtree(false);
    // Don't cover |icon| when the ink drops are being painted.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  }
};

BEGIN_METADATA(IconWrapper, views::View)
END_METADATA

}  // namespace

WebAuthnHoverButton::WebAuthnHoverButton(
    PressedCallback callback,
    std::unique_ptr<views::ImageView> icon,
    const std::u16string& title_text,
    const std::u16string& subtitle_text,
    std::unique_ptr<views::View> secondary_icon,
    bool force_two_line)
    : HoverButton(std::move(callback), std::u16string()) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  auto* layout = SetLayoutManager(std::make_unique<views::TableLayout>());

  // TODO (crbug/1267403): This is hideous. We have to tell the TableLayout to
  // ignore the child views created by the LabelButton ancestor. They're not
  // used but must exist to keep things happy. This view should be refactored to
  // descend from views::Button directly.
  for (auto* child : children())
    layout->SetChildViewIgnoredByLayout(child, true);

  const int icon_padding = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  if (icon) {
    layout
        ->AddColumn(views::LayoutAlignment::kCenter,
                    views::LayoutAlignment::kCenter,
                    views::TableLayout::kFixedSize,
                    views::TableLayout::ColumnSize::kUsePreferred,
                    /*fixed_width=*/0,
                    /*min_width=*/0)
        .AddPaddingColumn(views::TableLayout::kFixedSize, icon_padding);
  }
  layout->AddColumn(
      views::LayoutAlignment::kStretch, views::LayoutAlignment::kStretch,
      /*horizontal_resize=*/1.0, views::TableLayout::ColumnSize::kUsePreferred,
      /*fixed_width=*/0,
      /*min_width=*/0);
  if (secondary_icon) {
    layout->AddPaddingColumn(views::TableLayout::kFixedSize, icon_padding)
        .AddColumn(views::LayoutAlignment::kCenter,
                   views::LayoutAlignment::kCenter,
                   views::TableLayout::kFixedSize,
                   views::TableLayout::ColumnSize::kUsePreferred,
                   /*fixed_width=*/0,
                   /*min_width=*/0);
  }

  const int row_height = views::style::GetLineHeight(
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY);
  const bool is_two_line = !subtitle_text.empty() || force_two_line;
  const int icon_row_span = is_two_line ? 2 : 1;
  layout->AddRows(icon_row_span, views::TableLayout::kFixedSize, row_height);

  if (icon) {
    icon_view_ = AddChildView(std::make_unique<IconWrapper>(std::move(icon)));
    icon_view_->SetProperty(views::kTableColAndRowSpanKey,
                            gfx::Size(/*width=*/1, icon_row_span));
  }

  const int title_row_span = force_two_line && subtitle_text.empty() ? 2 : 1;
  title_ = AddChildView(std::make_unique<views::Label>(title_text));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetProperty(views::kTableColAndRowSpanKey,
                      gfx::Size(/*width=*/1, title_row_span));

  if (secondary_icon) {
    secondary_icon_view_ =
        AddChildView(std::make_unique<IconWrapper>(std::move(secondary_icon)));
    secondary_icon_view_->SetProperty(views::kTableColAndRowSpanKey,
                                      gfx::Size(/*width=*/1, icon_row_span));
  }

  if (is_two_line && !subtitle_text.empty()) {
    subtitle_ = AddChildView(std::make_unique<views::Label>(subtitle_text));
    subtitle_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }

  SetAccessibleName(subtitle_text.empty()
                        ? title_text
                        : base::JoinString({title_text, subtitle_text}, u"\n"));

  // Per WebAuthn UI specs, the top/bottom insets of hover buttons are 12dp for
  // a one-line button, and 8dp for a two-line button. Left/right insets are
  // 8dp assuming a 20dp primary icon, or no icon at all. (With a 24dp primary
  // icon, the left inset would be 12dp, but we don't currently have a button
  // with such an icon.)

  const int vert_inset = is_two_line ? 8 : 12;
  constexpr int horz_inset = 8;
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(vert_inset, horz_inset, vert_inset, horz_inset)));
}

BEGIN_METADATA(WebAuthnHoverButton, HoverButton)
END_METADATA
