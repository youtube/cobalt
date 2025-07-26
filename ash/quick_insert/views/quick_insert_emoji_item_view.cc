// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_emoji_item_view.h"

#include <string>
#include <utility>

#include "ash/ash_element_identifiers.h"
#include "ash/quick_insert/views/quick_insert_item_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

const gfx::FontList kEmojiFont({"Noto Color Emoji"},
                               gfx::Font::NORMAL,
                               20,
                               gfx::Font::Weight::NORMAL);
const gfx::FontList kEmoticonFont({"Google Sans", "Roboto"},
                                  gfx::Font::NORMAL,
                                  14,
                                  gfx::Font::Weight::NORMAL);
const gfx::FontList kSymbolFont({"Google Sans", "Roboto"},
                                gfx::Font::NORMAL,
                                16,
                                gfx::Font::Weight::NORMAL);
constexpr int kCornerRadius = 4;
constexpr auto kEmoticonItemMargins = gfx::Insets::VH(0, 6);

const gfx::FontList& GetFontForStyle(QuickInsertEmojiItemView::Style style) {
  switch (style) {
    case QuickInsertEmojiItemView::Style::kEmoji:
      return kEmojiFont;
    case QuickInsertEmojiItemView::Style::kEmoticon:
      return kEmoticonFont;
    case QuickInsertEmojiItemView::Style::kSymbol:
      return kSymbolFont;
  }
}

}  // namespace

QuickInsertEmojiItemView::QuickInsertEmojiItemView(
    Style style,
    SelectItemCallback select_item_callback,
    const std::u16string& text)
    : QuickInsertItemView(std::move(select_item_callback)) {
  SetUseDefaultFillLayout(true);
  SetCornerRadius(kCornerRadius);
  SetProperty(views::kElementIdentifierKey, kQuickInsertEmojiItemElementId);

  label_ = AddChildView(views::Builder<views::Label>()
                            .SetText(text)
                            .SetFontList(GetFontForStyle(style))
                            .Build());

  if (style == Style::kEmoticon || style == Style::kSymbol) {
    label_->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  }
  if (style == Style::kEmoticon) {
    label_->SetBorder(views::CreateEmptyBorder(kEmoticonItemMargins));
  }

  GetViewAccessibility().SetName(*label_);
}

std::u16string_view QuickInsertEmojiItemView::GetTextForTesting() const {
  return label_->GetText();
}

QuickInsertEmojiItemView::~QuickInsertEmojiItemView() = default;

BEGIN_METADATA(QuickInsertEmojiItemView)
END_METADATA

}  // namespace ash
