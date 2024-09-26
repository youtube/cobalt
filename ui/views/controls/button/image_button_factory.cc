// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ui/views/controls/button/image_button_factory.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ref.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/painter.h"

namespace views {

namespace {

class ColorTrackingVectorImageButton : public ImageButton {
 public:
  ColorTrackingVectorImageButton(PressedCallback callback,
                                 const gfx::VectorIcon& icon,
                                 int dip_size)
      : ImageButton(std::move(callback)), icon_(icon), dip_size_(dip_size) {}

  // ImageButton:
  void OnThemeChanged() override {
    ImageButton::OnThemeChanged();
    const ui::ColorProvider* cp = GetColorProvider();
    const SkColor color = cp->GetColor(ui::kColorIcon);
    const SkColor disabled_color = cp->GetColor(ui::kColorIconDisabled);
    SetImageFromVectorIconWithColor(this, *icon_, dip_size_, color,
                                    disabled_color);
  }

 private:
  const raw_ref<const gfx::VectorIcon> icon_;
  int dip_size_;
};

}  // namespace

std::unique_ptr<ImageButton> CreateVectorImageButtonWithNativeTheme(
    Button::PressedCallback callback,
    const gfx::VectorIcon& icon,
    absl::optional<int> dip_size) {
  // We can't use `value_or` as that ALWAYS evaluates the false case, which is
  // undefined for some valid and commonly used Chrome vector icons.
  const int dip_size_value = dip_size.has_value()
                                 ? dip_size.value()
                                 : GetDefaultSizeOfVectorIcon(icon);

  auto button = std::make_unique<ColorTrackingVectorImageButton>(
      std::move(callback), icon, dip_size_value);
  ConfigureVectorImageButton(button.get());
  return button;
}

std::unique_ptr<ImageButton> CreateVectorImageButton(
    Button::PressedCallback callback) {
  auto button = std::make_unique<ImageButton>(std::move(callback));
  ConfigureVectorImageButton(button.get());
  return button;
}

std::unique_ptr<ToggleImageButton> CreateVectorToggleImageButton(
    Button::PressedCallback callback) {
  auto button = std::make_unique<ToggleImageButton>(std::move(callback));
  ConfigureVectorImageButton(button.get());
  return button;
}

void ConfigureVectorImageButton(ImageButton* button) {
  InkDrop::Get(button)->SetMode(views::InkDropHost::InkDropMode::ON);
  button->SetHasInkDropActionOnClick(true);
  button->SetImageHorizontalAlignment(ImageButton::ALIGN_CENTER);
  button->SetImageVerticalAlignment(ImageButton::ALIGN_MIDDLE);
  button->SetBorder(CreateEmptyBorder(
      LayoutProvider::Get()->GetInsetsMetric(INSETS_VECTOR_IMAGE_BUTTON)));
}

void SetImageFromVectorIconWithColor(ImageButton* button,
                                     const gfx::VectorIcon& icon,
                                     SkColor icon_color,
                                     SkColor icon_disabled_color) {
  SetImageFromVectorIconWithColor(button, icon,
                                  GetDefaultSizeOfVectorIcon(icon), icon_color,
                                  icon_disabled_color);
}

void SetImageFromVectorIconWithColor(ImageButton* button,
                                     const gfx::VectorIcon& icon,
                                     int dip_size,
                                     SkColor icon_color,
                                     SkColor icon_disabled_color) {
  const ui::ImageModel& normal_image =
      ui::ImageModel::FromVectorIcon(icon, icon_color, dip_size);
  const ui::ImageModel& disabled_image =
      ui::ImageModel::FromVectorIcon(icon, icon_disabled_color, dip_size);

  button->SetImageModel(Button::STATE_NORMAL, normal_image);
  button->SetImageModel(Button::STATE_DISABLED, disabled_image);
  InkDrop::Get(button)->SetBaseColor(icon_color);
}

void SetToggledImageFromVectorIconWithColor(ToggleImageButton* button,
                                            const gfx::VectorIcon& icon,
                                            int dip_size,
                                            SkColor icon_color,
                                            SkColor disabled_color) {
  const ui::ImageModel& normal_image =
      ui::ImageModel::FromVectorIcon(icon, icon_color, dip_size);
  const ui::ImageModel& disabled_image =
      ui::ImageModel::FromVectorIcon(icon, disabled_color, dip_size);

  button->SetToggledImageModel(Button::STATE_NORMAL, normal_image);
  button->SetToggledImageModel(Button::STATE_DISABLED, disabled_image);
}

void SetImageFromVectorIconWithColorId(
    ImageButton* button,
    const gfx::VectorIcon& icon,
    ui::ColorId icon_color_id,
    ui::ColorId icon_disabled_color_id,
    absl::optional<int> icon_size /*=nullopt*/) {
  int dip_size = icon_size.has_value() ? icon_size.value()
                                       : GetDefaultSizeOfVectorIcon(icon);
  const ui::ImageModel& normal_image =
      ui::ImageModel::FromVectorIcon(icon, icon_color_id, dip_size);
  const ui::ImageModel& disabled_image =
      ui::ImageModel::FromVectorIcon(icon, icon_disabled_color_id, dip_size);

  button->SetImageModel(Button::STATE_NORMAL, normal_image);
  button->SetImageModel(Button::STATE_DISABLED, disabled_image);
  InkDrop::Get(button)->SetBaseColorId(icon_color_id);
}

void SetToggledImageFromVectorIconWithColorId(
    ToggleImageButton* button,
    const gfx::VectorIcon& icon,
    ui::ColorId icon_color_id,
    ui::ColorId icon_disabled_color_id) {
  int dip_size = GetDefaultSizeOfVectorIcon(icon);
  const ui::ImageModel& normal_image =
      ui::ImageModel::FromVectorIcon(icon, icon_color_id, dip_size);
  const ui::ImageModel& disabled_image =
      ui::ImageModel::FromVectorIcon(icon, icon_disabled_color_id, dip_size);

  button->SetToggledImageModel(Button::STATE_NORMAL, normal_image);
  button->SetToggledImageModel(Button::STATE_DISABLED, disabled_image);
}

}  // namespace views
