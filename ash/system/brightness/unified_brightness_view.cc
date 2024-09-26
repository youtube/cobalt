// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/unified_brightness_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/brightness/unified_brightness_slider_controller.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "ash/system/tray/tray_constants.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

namespace {

// The maximum index of `kBrightnessLevelIcons`.
const int kBrightnessLevels =
    std::size(UnifiedBrightnessView::kBrightnessLevelIcons) - 1;

// Get vector icon reference that corresponds to the given brightness level.
// `level` is between 0.0 to 1.0.
const gfx::VectorIcon& GetBrightnessIconForLevel(float level) {
  int index = static_cast<int>(std::ceil(level * kBrightnessLevels));
  DCHECK(index >= 0 && index <= kBrightnessLevels);
  return *UnifiedBrightnessView::kBrightnessLevelIcons[index];
}

}  // namespace

UnifiedBrightnessView::UnifiedBrightnessView(
    UnifiedBrightnessSliderController* controller,
    scoped_refptr<UnifiedSystemTrayModel> model,
    absl::optional<views::Button::PressedCallback> detailed_button_callback)
    : UnifiedSliderView(views::Button::PressedCallback(),
                        controller,
                        kUnifiedMenuBrightnessIcon,
                        IDS_ASH_STATUS_TRAY_BRIGHTNESS),
      model_(model),
      night_light_controller_(Shell::Get()->night_light_controller()) {
  model_->AddObserver(this);

  if (features::IsQsRevampEnabled()) {
    // For QsRevamp: This case applies to the brightness slider in the
    // `DisplayDetailedView`. If `detailed_button_callback` is not passed in,
    // both the `night_light_button_` and the drill-in button will not be added.
    if (!detailed_button_callback.has_value()) {
      OnDisplayBrightnessChanged(/*by_user=*/false);
      return;
    }

    const bool enabled = night_light_controller_->GetEnabled();
    night_light_button_ = AddChildView(std::make_unique<IconButton>(
        base::BindRepeating(&UnifiedBrightnessView::OnNightLightButtonPressed,
                            base::Unretained(this)),
        IconButton::Type::kMedium,
        enabled ? &kUnifiedMenuNightLightIcon : &kUnifiedMenuNightLightOffIcon,
        l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_TOGGLE_TOOLTIP,
            l10n_util::GetStringUTF16(
                enabled
                    ? IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_ENABLED_STATE_TOOLTIP
                    : IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_DISABLED_STATE_TOOLTIP)),
        /*is_togglable=*/true,
        /*has_border=*/true));
    // Sets the icon, icon color, background color for `night_light_button_`
    // when it's toggled.
    night_light_button_->SetToggledVectorIcon(kUnifiedMenuNightLightIcon);
    night_light_button_->SetIconToggledColorId(
        cros_tokens::kCrosSysSystemOnPrimaryContainer);
    night_light_button_->SetBackgroundToggledColorId(
        cros_tokens::kCrosSysSystemPrimaryContainer);
    // Sets the icon, icon color, background color for `night_light_button_`
    // when it's not toggled.
    night_light_button_->SetVectorIcon(kUnifiedMenuNightLightOffIcon);
    night_light_button_->SetIconColorId(cros_tokens::kCrosSysOnSurface);
    night_light_button_->SetBackgroundColorId(
        cros_tokens::kCrosSysSystemOnBase);

    night_light_button_->SetToggled(enabled);

    auto* more_button = AddChildView(std::make_unique<IconButton>(
        std::move(detailed_button_callback.value()),
        IconButton::Type::kMediumFloating, &kQuickSettingsRightArrowIcon,
        IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_SETTINGS_TOOLTIP));
    more_button->SetIconColorId(cros_tokens::kCrosSysSecondary);
  } else {
    button()->SetEnabled(false);
    // The button is set to disabled but wants to keep the color for an enabled
    // icon.
    button()->SetImageModel(
        views::Button::STATE_DISABLED,
        ui::ImageModel::FromVectorIcon(kUnifiedMenuBrightnessIcon,
                                       kColorAshButtonIconColor));
  }
  OnDisplayBrightnessChanged(/*by_user=*/false);
}

UnifiedBrightnessView::~UnifiedBrightnessView() {
  model_->RemoveObserver(this);
}

void UnifiedBrightnessView::OnDisplayBrightnessChanged(bool by_user) {
  float level = model_->display_brightness();

  if (features::IsQsRevampEnabled()) {
    slider_icon()->SetImage(ui::ImageModel::FromVectorIcon(
        GetBrightnessIconForLevel(level),
        cros_tokens::kCrosSysSystemOnPrimaryContainer, kQsSliderIconSize));
  }
  SetSliderValue(level, by_user);
}

void UnifiedBrightnessView::OnNightLightButtonPressed() {
  night_light_controller_->Toggle();

  UpdateNightLightButton();
}

void UnifiedBrightnessView::UpdateNightLightButton() {
  const bool enabled = night_light_controller_->GetEnabled();

  // Sets `night_light_button_` toggle state to update its icon, icon color,
  // and background color.
  night_light_button_->SetToggled(enabled);

  // Updates the tooltip of `night_light_button_`.
  std::u16string toggle_tooltip = l10n_util::GetStringUTF16(
      enabled ? IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_ENABLED_STATE_TOOLTIP
              : IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_DISABLED_STATE_TOOLTIP);
  night_light_button_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_TOGGLE_TOOLTIP, toggle_tooltip));
}

void UnifiedBrightnessView::VisibilityChanged(View* starting_from,
                                              bool is_visible) {
  OnDisplayBrightnessChanged(/*by_user=*/false);
  // Only updates the `night_light_button_` if in the main page.
  if (night_light_button_) {
    UpdateNightLightButton();
  }
}

BEGIN_METADATA(UnifiedBrightnessView, views::View)
END_METADATA

}  // namespace ash
