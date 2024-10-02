// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/unified_volume_view.h"

#include <memory>
#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_state.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// References to the icons that correspond to different volume levels.
const gfx::VectorIcon* const kVolumeLevelIcons[] = {
    &kUnifiedMenuVolumeLowIcon,     // Low volume.
    &kUnifiedMenuVolumeMediumIcon,  // Medium volume.
    &kUnifiedMenuVolumeHighIcon,    // High volume.
    &kUnifiedMenuVolumeHighIcon,    // Full volume.
};

// The maximum index of `kVolumeLevelIcons`.
constexpr int kVolumeLevels = std::size(kVolumeLevelIcons) - 1;

// The maximum index of `kQsVolumeLevelIcons`.
constexpr int kQsVolumeLevels =
    std::size(UnifiedVolumeView::kQsVolumeLevelIcons) - 1;

// Get vector icon reference that corresponds to the given volume level. `level`
// is between 0.0 to 1.0 inclusive.
const gfx::VectorIcon& GetVolumeIconForLevel(float level) {
  if (!features::IsQsRevampEnabled()) {
    int index = static_cast<int>(std::ceil(level * kVolumeLevels));
    DCHECK(index >= 0 && index <= kVolumeLevels);
    return *kVolumeLevelIcons[index];
  }

  int index = static_cast<int>(std::ceil(level * kQsVolumeLevels));
  DCHECK(index >= 0 && index <= kQsVolumeLevels);
  return *UnifiedVolumeView::kQsVolumeLevelIcons[index];
}

}  // namespace

UnifiedVolumeView::UnifiedVolumeView(
    UnifiedVolumeSliderController* controller,
    UnifiedVolumeSliderController::Delegate* delegate,
    bool is_active_output_node)
    : UnifiedSliderView(base::BindRepeating(
                            &UnifiedVolumeSliderController::SliderButtonPressed,
                            base::Unretained(controller)),
                        controller,
                        kSystemMenuVolumeHighIcon,
                        IDS_ASH_STATUS_TRAY_VOLUME_SLIDER_LABEL),
      more_button_(AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(&UnifiedVolumeSliderController::Delegate::
                                  OnAudioSettingsButtonClicked,
                              delegate->weak_ptr_factory_.GetWeakPtr()),
          features::IsQsRevampEnabled() ? IconButton::Type::kMediumFloating
                                        : IconButton::Type::kMedium,
          &kQuickSettingsRightArrowIcon,
          IDS_ASH_STATUS_TRAY_AUDIO))),
      is_active_output_node_(is_active_output_node),
      slider_style_(QuickSettingsSlider::Style::kDefault),
      a11y_controller_(Shell::Get()->accessibility_controller()),
      device_id_(CrasAudioHandler::Get()->GetPrimaryActiveOutputNode()) {
  CrasAudioHandler::Get()->AddAudioObserver(this);

  // In the case that there is a trusted pinned window (fullscreen lock mode)
  // and the volume slider popup is shown, do not allow the more_button_ to
  // open quick settings.
  auto* window = Shell::Get()->screen_pinning_controller()->pinned_window();
  if (window && WindowState::Get(window)->IsTrustedPinned()) {
    more_button_->SetEnabled(false);
  }

  if (features::IsQsRevampEnabled()) {
    more_button_->SetIconColorId(cros_tokens::kCrosSysSecondary);
    // TODO(b/257151067): Update the a11y name id.
    // Adds the live caption button before `more_button_`.
    a11y_controller_->AddObserver(this);
    const bool enabled = a11y_controller_->live_caption().enabled();
    live_caption_button_ = AddChildViewAt(
        std::make_unique<IconButton>(
            base::BindRepeating(&UnifiedVolumeView::OnLiveCaptionButtonPressed,
                                base::Unretained(this)),
            IconButton::Type::kMedium,
            enabled ? &kUnifiedMenuLiveCaptionIcon
                    : &kUnifiedMenuLiveCaptionOffIcon,
            l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_LIVE_CAPTION_TOGGLE_TOOLTIP,
                l10n_util::GetStringUTF16(
                    enabled
                        ? IDS_ASH_STATUS_TRAY_LIVE_CAPTION_ENABLED_STATE_TOOLTIP
                        : IDS_ASH_STATUS_TRAY_LIVE_CAPTION_DISABLED_STATE_TOOLTIP)),
            /*is_togglable=*/true,
            /*has_border=*/true),
        GetIndexOf(more_button_).value());
    // Sets the icon, icon color, background color for `live_caption_button_`
    // when it's toggled.
    live_caption_button_->SetToggledVectorIcon(kUnifiedMenuLiveCaptionIcon);
    live_caption_button_->SetIconToggledColorId(
        cros_tokens::kCrosSysSystemOnPrimaryContainer);
    live_caption_button_->SetBackgroundToggledColorId(
        cros_tokens::kCrosSysSystemPrimaryContainer);
    // Sets the icon, icon color, background color for `live_caption_button_`
    // when it's not toggled.
    live_caption_button_->SetVectorIcon(kUnifiedMenuLiveCaptionOffIcon);
    live_caption_button_->SetIconColorId(cros_tokens::kCrosSysOnSurface);
    live_caption_button_->SetBackgroundColorId(
        cros_tokens::kCrosSysSystemOnBase);

    live_caption_button_->SetToggled(enabled);
  }

  Update(/*by_user=*/false);
}

UnifiedVolumeView::UnifiedVolumeView(UnifiedVolumeSliderController* controller,
                                     uint64_t device_id,
                                     bool is_active_output_node)
    : UnifiedSliderView(base::BindRepeating(
                            &UnifiedVolumeSliderController::SliderButtonPressed,
                            base::Unretained(controller)),
                        controller,
                        kSystemMenuVolumeHighIcon,
                        IDS_ASH_STATUS_TRAY_VOLUME_SLIDER_LABEL,
                        false,
                        QuickSettingsSlider::Style::kRadioActive),
      more_button_(nullptr),
      is_active_output_node_(is_active_output_node),
      slider_style_(QuickSettingsSlider::Style::kRadioActive),
      a11y_controller_(Shell::Get()->accessibility_controller()),
      device_id_(device_id) {
  CrasAudioHandler::Get()->AddAudioObserver(this);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kRadioSliderViewPadding,
      kSliderChildrenViewSpacing));
  slider()->SetBorder(views::CreateEmptyBorder(kRadioSliderPadding));
  slider()->SetPreferredSize(kRadioSliderPreferredSize);
  slider_icon()->SetBorder(views::CreateEmptyBorder(kRadioSliderIconPadding));
  layout->SetFlexForView(slider()->parent(), /*flex=*/1);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetPreferredSize(kRadioSliderPreferredSize);

  Update(/*by_user=*/false);
}

UnifiedVolumeView::~UnifiedVolumeView() {
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
  if (features::IsQsRevampEnabled()) {
    a11y_controller_->RemoveObserver(this);
  }
}

void UnifiedVolumeView::Update(bool by_user) {
  auto* audio_handler = CrasAudioHandler::Get();
  float level = audio_handler->GetOutputVolumePercent() / 100.f;

  if (!features::IsQsRevampEnabled()) {
    bool is_muted = CrasAudioHandler::Get()->IsOutputMuted();
    // To indicate that the volume is muted, set the volume slider to the
    // minimal visual style.
    slider()->SetRenderingStyle(
        is_muted ? views::Slider::RenderingStyle::kMinimalStyle
                 : views::Slider::RenderingStyle::kDefaultStyle);
    slider()->SetEnabled(!CrasAudioHandler::Get()->IsOutputMutedByPolicy());

    // The button should be gray when muted and colored otherwise.
    button()->SetToggled(!is_muted);
    button()->SetVectorIcon(is_muted ? kUnifiedMenuVolumeMuteIcon
                                     : GetVolumeIconForLevel(level));
    std::u16string state_tooltip_text = l10n_util::GetStringUTF16(
        is_muted ? IDS_ASH_STATUS_TRAY_VOLUME_STATE_MUTED
                 : IDS_ASH_STATUS_TRAY_VOLUME_STATE_ON);
    button()->SetTooltipText(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_VOLUME, state_tooltip_text));
  } else {
    level = audio_handler->GetOutputVolumePercentForDevice(device_id_) / 100.f;
    // When muted by keyboard, the level stored in `audio_handler` should be
    // preserved but the slider should appear as muted. `level` is set to 0
    // manually to update the slider and icon.
    if (audio_handler->IsOutputMutedForDevice(device_id_)) {
      level = 0;
    }
    auto active_device_id = audio_handler->GetPrimaryActiveOutputNode();

    switch (slider_style_) {
      case QuickSettingsSlider::Style::kDefault: {
        // TODO(b/257151067): Adds tooltip.
        slider_icon()->SetImage(ui::ImageModel::FromVectorIcon(
            GetVolumeIconForLevel(level),
            cros_tokens::kCrosSysSystemOnPrimaryContainer, kQsSliderIconSize));
        break;
      }
      case QuickSettingsSlider::Style::kRadioActive:
      case QuickSettingsSlider::Style::kRadioInactive: {
        static_cast<QuickSettingsSlider*>(slider())->SetSliderStyle(
            active_device_id == device_id_
                ? QuickSettingsSlider::Style::kRadioActive
                : QuickSettingsSlider::Style::kRadioInactive);

        slider_icon()->SetImage(ui::ImageModel::FromVectorIcon(
            GetVolumeIconForLevel(level),
            active_device_id == device_id_
                ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                : cros_tokens::kCrosSysSecondary,
            kQsSliderIconSize));
        break;
      }
      default:
        NOTREACHED();
    }
  }

  // Slider's value is in finer granularity than audio volume level(0.01),
  // there will be a small discrepancy between slider's value and volume level
  // on audio side. To avoid the jittering in slider UI, use the slider's
  // current value.
  if (level != 1.0 && std::abs(level - slider()->GetValue()) <
                          kAudioSliderIgnoreUpdateThreshold) {
    level = slider()->GetValue();
  }
  // Note: even if the value does not change, we still need to call this
  // function to enable accessibility events (crbug.com/1013251).
  SetSliderValue(level, by_user);
}

void UnifiedVolumeView::OnLiveCaptionButtonPressed() {
  a11y_controller_->live_caption().SetEnabled(
      !a11y_controller_->live_caption().enabled());
}

void UnifiedVolumeView::OnOutputNodeVolumeChanged(uint64_t node_id,
                                                  int volume) {
  Update(/*by_user=*/true);
}

void UnifiedVolumeView::OnOutputMuteChanged(bool mute_on) {
  Update(/*by_user=*/true);
}

void UnifiedVolumeView::OnAudioNodesChanged() {
  Update(/*by_user=*/true);
}

void UnifiedVolumeView::OnActiveOutputNodeChanged() {
  // If this view is for the active output node, we need to update the
  // `device_id_` before repaint.
  if (is_active_output_node_) {
    device_id_ = CrasAudioHandler::Get()->GetPrimaryActiveOutputNode();
  }

  Update(/*by_user=*/true);
}

void UnifiedVolumeView::OnActiveInputNodeChanged() {
  Update(/*by_user=*/true);
}

void UnifiedVolumeView::OnAccessibilityStatusChanged() {
  if (!features::IsQsRevampEnabled()) {
    return;
  }

  const bool enabled = a11y_controller_->live_caption().enabled();

  // Sets `live_caption_button_` toggle state to update its icon, icon color,
  // and background color.
  live_caption_button_->SetToggled(enabled);

  // Updates the tooltip of `live_caption_button_`.
  std::u16string toggle_tooltip = l10n_util::GetStringUTF16(
      enabled ? IDS_ASH_STATUS_TRAY_LIVE_CAPTION_ENABLED_STATE_TOOLTIP
              : IDS_ASH_STATUS_TRAY_LIVE_CAPTION_DISABLED_STATE_TOOLTIP);

  live_caption_button_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_LIVE_CAPTION_TOGGLE_TOOLTIP, toggle_tooltip));
}

void UnifiedVolumeView::ChildVisibilityChanged(views::View* child) {
  Layout();
}

void UnifiedVolumeView::VisibilityChanged(View* starting_from,
                                          bool is_visible) {
  Update(/*by_user=*/true);
}

BEGIN_METADATA(UnifiedVolumeView, views::View)
END_METADATA

}  // namespace ash
