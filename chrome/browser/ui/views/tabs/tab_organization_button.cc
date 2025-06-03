// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_organization_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kTabOrganizeCornerRadius = 10;
constexpr int kTabOrganizeFlatCornerRadius = 2;
constexpr int kTabOrganizeLabelMargin = 10;
constexpr int kTabOrganizeCloseButtonMargin = 8;
constexpr int kTabOrganizeCloseButtonSize = 16;
}

TabOrganizationButton::TabOrganizationButton(
    TabStripController* tab_strip_controller,
    TabOrganizationService* tab_organization_service,
    PressedCallback pressed_callback,
    Edge flat_edge)
    : TabStripControlButton(
          tab_strip_controller,
          base::BindRepeating(&TabOrganizationButton::ButtonPressed,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(IDS_TAB_ORGANIZE),
          flat_edge),
      service_(tab_organization_service),
      pressed_callback_(std::move(pressed_callback)),
      browser_(tab_strip_controller->GetBrowser()) {
  auto* const layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);

  SetProperty(views::kElementIdentifierKey, kTabOrganizationButtonElementId);

  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ORGANIZE));
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_ORGANIZE));
  SetLabelStyle(views::style::STYLE_BODY_3_EMPHASIS);
  label()->SetElideBehavior(gfx::ElideBehavior::NO_ELIDE);

  const gfx::Insets label_margin =
      gfx::Insets().set_left(kTabOrganizeLabelMargin);
  label()->SetProperty(views::kMarginsKey, label_margin);

  SetForegroundFrameActiveColorId(kColorNewTabButtonForegroundFrameActive);
  SetForegroundFrameInactiveColorId(kColorNewTabButtonForegroundFrameInactive);
  SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  SetBackgroundFrameInactiveColorId(
      kColorNewTabButtonCRBackgroundFrameInactive);

  set_paint_transparent_for_custom_image_theme(false);

  SetCloseButton(base::BindRepeating(&TabOrganizationButton::ClosePressed,
                                     base::Unretained(this)));
  layout_manager->SetFlexForView(close_button_, 1);

  UpdateColors();
}

TabOrganizationButton::~TabOrganizationButton() = default;

void TabOrganizationButton::SetWidthFactor(float factor) {
  width_factor_ = factor;
  PreferredSizeChanged();
}

gfx::Size TabOrganizationButton::CalculatePreferredSize() const {
  const int full_width = GetLayoutManager()->GetPreferredSize(this).width();
  const int width = full_width * width_factor_;
  const int height = TabStripControlButton::CalculatePreferredSize().height();
  return gfx::Size(width, height);
}

void TabOrganizationButton::ButtonPressed(const ui::Event& event) {
  if (service_ && browser_) {
    service_->StartRequest(browser_);
  }
  pressed_callback_.Run(event);
}

void TabOrganizationButton::ClosePressed(const ui::Event& event) {
  pressed_callback_.Run(event);
}

int TabOrganizationButton::GetCornerRadius() const {
  return kTabOrganizeCornerRadius;
}

int TabOrganizationButton::GetFlatCornerRadius() const {
  return kTabOrganizeFlatCornerRadius;
}

void TabOrganizationButton::SetCloseButton(
    views::LabelButton::PressedCallback pressed_callback) {
  auto close_button =
      std::make_unique<views::LabelButton>(std::move(pressed_callback));
  close_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ORGANIZE_CLOSE));

  const ui::ImageModel icon_image_model = ui::ImageModel::FromVectorIcon(
      vector_icons::kCloseChromeRefreshIcon,
      kColorNewTabButtonForegroundFrameActive, kTabOrganizeCloseButtonSize);

  close_button->SetImageModel(views::Button::STATE_NORMAL, icon_image_model);
  close_button->SetImageModel(views::Button::STATE_HOVERED, icon_image_model);
  close_button->SetImageModel(views::Button::STATE_PRESSED, icon_image_model);

  close_button->SetPaintToLayer();
  close_button->layer()->SetFillsBoundsOpaquely(false);

  views::InkDrop::Get(close_button.get())
      ->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(close_button.get())->SetHighlightOpacity(0.16f);
  views::InkDrop::Get(close_button.get())->SetVisibleOpacity(0.14f);
  views::InkDrop::Get(close_button.get())
      ->SetBaseColorId(kColorNewTabButtonForegroundFrameActive);

  auto ink_drop_highlight_path =
      std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets());
  views::HighlightPathGenerator::Install(close_button.get(),
                                         std::move(ink_drop_highlight_path));

  close_button->SetPreferredSize(
      gfx::Size(kTabOrganizeCloseButtonSize, kTabOrganizeCloseButtonSize));
  close_button->SetBorder(nullptr);

  const gfx::Insets margin = gfx::Insets().set_left_right(
      kTabOrganizeCloseButtonMargin, kTabOrganizeCloseButtonMargin);
  close_button->SetProperty(views::kMarginsKey, margin);

  close_button_ = AddChildView(std::move(close_button));
}

BEGIN_METADATA(TabOrganizationButton, TabStripControlButton)
END_METADATA
