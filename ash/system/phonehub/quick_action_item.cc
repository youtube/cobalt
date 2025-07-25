// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/quick_action_item.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/typography.h"
#include "ash/system/tray/tray_constants.h"
#include "base/functional/bind.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

void ConfigureLabelProperties(views::Label* label) {
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  label->SetCanProcessEventsWithinSubtree(false);
}

void ConfigureLabelFonts(views::Label* label, int line_height, int font_size) {
  gfx::Font default_font;
  gfx::Font label_font =
      default_font.Derive(font_size - default_font.GetFontSize(),
                          gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);
  gfx::FontList font_list(label_font);
  label->SetFontList(font_list);
  label->SetLineHeight(line_height);
}

}  // namespace

QuickActionItem::QuickActionItem(Delegate* delegate,
                                 int label_id,
                                 const gfx::VectorIcon& icon) {
  SetPreferredSize(kUnifiedFeaturePodSize);
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kUnifiedFeaturePodSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  icon_button_ = AddChildView(std::make_unique<FeaturePodIconButton>(
      base::BindRepeating(
          [](Delegate* delegate, QuickActionItem* item) {
            delegate->OnButtonPressed(item->IsToggled());
          },
          delegate, this),
      true /* is_togglable */));
  icon_button_->SetVectorIcon(icon);

  auto* label_view = AddChildView(std::make_unique<views::View>());
  label_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets()));

  label_ = label_view->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(label_id)));
  label_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, 0, kUnifiedFeaturePodInterLabelPadding, 0)));
  sub_label_ = label_view->AddChildView(std::make_unique<views::Label>());

  ConfigureLabelProperties(label_);
  ConfigureLabelProperties(sub_label_);

  if (chromeos::features::IsJellyrollEnabled()) {
    // StyleLabel() will configure the height, weight, font, etc.
    TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosButton2,
                                          *label_);
    TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosBody2,
                                          *sub_label_);
    sub_label_color_ = GetColorProvider()
                           ? GetColorProvider()->GetColor(
                                 cros_tokens::kCrosSysOnSurfaceVariant)
                           : gfx::kPlaceholderColor;
  } else {
    ConfigureLabelFonts(label_, kUnifiedFeaturePodLabelLineHeight,
                        kUnifiedFeaturePodLabelFontSize);
    ConfigureLabelFonts(sub_label_, kUnifiedFeaturePodSubLabelLineHeight,
                        kUnifiedFeaturePodSubLabelFontSize);
    sub_label_color_ = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorSecondary);
  }

  SetEnabled(true /* enabled */);
}

QuickActionItem::~QuickActionItem() = default;

void QuickActionItem::SetSubLabel(const std::u16string& sub_label) {
  sub_label_->SetText(sub_label);
}

void QuickActionItem::SetSubLabelColor(SkColor color) {
  if (sub_label_color_ == color)
    return;
  sub_label_color_ = color;
  sub_label_->SetEnabledColor(sub_label_color_);
}

void QuickActionItem::SetTooltip(const std::u16string& text) {
  icon_button_->SetTooltipText(text);
}

void QuickActionItem::SetToggled(bool toggled) {
  icon_button_->SetToggled(toggled);
}

bool QuickActionItem::IsToggled() const {
  return icon_button_->toggled();
}

const std::u16string& QuickActionItem::GetItemLabel() const {
  return label_->GetText();
}

void QuickActionItem::SetEnabled(bool enabled) {
  View::SetEnabled(enabled);
  icon_button_->SetEnabled(enabled);
  if (chromeos::features::IsJellyrollEnabled() && !GetColorProvider()) {
    return;
  }

  // When creating QuickActionItem |sub_label_color_| may have been set to
  // gfx::kPlaceholderColor, if color provider was null and Jellyroll enabled,
  // update color here.
  sub_label_color_ =
      chromeos::features::IsJellyrollEnabled()
          ? GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurfaceVariant)
          : AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kTextColorSecondary);

  if (!enabled) {
    label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorSecondary));
    if (chromeos::features::IsJellyrollEnabled()) {
      sub_label_->SetEnabledColor(
          GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurfaceVariant));
    } else {
      sub_label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorSecondary));
    }

    sub_label_->SetText(l10n_util::GetStringUTF16(
        IDS_ASH_PHONE_HUB_QUICK_ACTIONS_NOT_AVAILABLE_STATE));
    icon_button_->SetTooltipText(l10n_util::GetStringFUTF16(
        IDS_ASH_PHONE_HUB_QUICK_ACTIONS_NOT_AVAILABLE_STATE_TOOLTIP,
        GetItemLabel()));
  } else {
    label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
    sub_label_->SetEnabledColor(sub_label_color_);
  }
}

bool QuickActionItem::HasFocus() const {
  return icon_button_->HasFocus() || label_->HasFocus() ||
         sub_label_->HasFocus();
}

void QuickActionItem::RequestFocus() {
  icon_button_->RequestFocus();
}

const char* QuickActionItem::GetClassName() const {
  return "QuickActionItem";
}

}  // namespace ash
