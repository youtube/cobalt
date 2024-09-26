// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/audio_effects_controller.h"

#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/notreached.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/live_caption/caption_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

AudioEffectsController::AudioEffectsController() {
  auto* session_controller = Shell::Get()->session_controller();
  DCHECK(session_controller);
  session_observation_.Observe(session_controller);
  CrasAudioHandler::Get()->AddAudioObserver(this);
}

AudioEffectsController::~AudioEffectsController() {
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
  VideoConferenceTrayEffectsManager& effects_manager =
      VideoConferenceTrayController::Get()->effects_manager();
  if (effects_manager.IsDelegateRegistered(this)) {
    effects_manager.UnregisterDelegate(this);
  }
}

bool IsNoiseCancellationSupported() {
  auto* cras_audio_handler = CrasAudioHandler::Get();
  return cras_audio_handler->IsNoiseCancellationSupportedForDevice(
      cras_audio_handler->GetPrimaryActiveInputNode());
}

bool AudioEffectsController::IsEffectSupported(VcEffectId effect_id) {
  switch (effect_id) {
    case VcEffectId::kNoiseCancellation:
      return IsNoiseCancellationSupported();
    case VcEffectId::kLiveCaption:
      return captions::IsLiveCaptionFeatureSupported();
    case VcEffectId::kBackgroundBlur:
    case VcEffectId::kPortraitRelighting:
    case VcEffectId::kTestEffect:
      NOTREACHED();
      return false;
  }
}

absl::optional<int> AudioEffectsController::GetEffectState(
    VcEffectId effect_id) {
  switch (effect_id) {
    case VcEffectId::kNoiseCancellation:
      return CrasAudioHandler::Get()->GetNoiseCancellationState() ? 1 : 0;
    case VcEffectId::kLiveCaption:
      return Shell::Get()->accessibility_controller()->live_caption().enabled()
                 ? 1
                 : 0;
    case VcEffectId::kBackgroundBlur:
    case VcEffectId::kPortraitRelighting:
    case VcEffectId::kTestEffect:
      NOTREACHED();
      return absl::nullopt;
  }
}

void AudioEffectsController::OnEffectControlActivated(
    VcEffectId effect_id,
    absl::optional<int> value) {
  switch (effect_id) {
    case VcEffectId::kNoiseCancellation: {
      // Toggle noise cancellation.
      CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
      bool new_state = !audio_handler->GetNoiseCancellationState();
      audio_handler->SetNoiseCancellationState(
          new_state,
          CrasAudioHandler::AudioSettingsChangeSource::kVideoConferenceTray);
      return;
    }
    case VcEffectId::kLiveCaption: {
      // Toggle live caption.
      AccessibilityControllerImpl* controller =
          Shell::Get()->accessibility_controller();
      controller->live_caption().SetEnabled(
          !controller->live_caption().enabled());
      return;
    }
    case VcEffectId::kBackgroundBlur:
    case VcEffectId::kPortraitRelighting:
    case VcEffectId::kTestEffect:
      NOTREACHED();
      return;
  }
}

void AudioEffectsController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  VideoConferenceTrayEffectsManager& effects_manager =
      VideoConferenceTrayController::Get()->effects_manager();

  // Invoked when the user initially logs in and on user switching in
  // multi-profile. If the delegate is already registered, no need to continue.
  if (effects_manager.IsDelegateRegistered(this)) {
    return;
  }

  noise_cancellation_supported_ =
      IsEffectSupported(VcEffectId::kNoiseCancellation);
  const bool live_caption_supported =
      IsEffectSupported(VcEffectId::kLiveCaption);

  if (noise_cancellation_supported_) {
    AddNoiseCancellationEffect();
  }

  if (live_caption_supported) {
    AddLiveCaptionEffect();
  }

  if (noise_cancellation_supported_ || live_caption_supported) {
    effects_manager.RegisterDelegate(this);
  }
}

void AudioEffectsController::OnActiveInputNodeChanged() {
  const bool noise_cancellation_supported = IsNoiseCancellationSupported();

  if (noise_cancellation_supported_ == noise_cancellation_supported) {
    return;
  }

  noise_cancellation_supported_ = noise_cancellation_supported;

  const bool effect_added = GetEffectById(VcEffectId::kNoiseCancellation);

  if (noise_cancellation_supported_ && !effect_added) {
    AddNoiseCancellationEffect();
  } else if (!noise_cancellation_supported_ && effect_added) {
    RemoveEffect(VcEffectId::kNoiseCancellation);
  }

  VideoConferenceTrayController::Get()
      ->effects_manager()
      .NotifyEffectSupportStateChanged(VcEffectId::kNoiseCancellation,
                                       noise_cancellation_supported_);
}

void AudioEffectsController::AddNoiseCancellationEffect() {
  std::unique_ptr<VcHostedEffect> effect = std::make_unique<VcHostedEffect>(
      /*type=*/VcEffectType::kToggle,
      /*get_state_callback=*/
      base::BindRepeating(&AudioEffectsController::GetEffectState,
                          base::Unretained(this),
                          VcEffectId::kNoiseCancellation),
      /*effect_id=*/VcEffectId::kNoiseCancellation);

  auto effect_state = std::make_unique<VcEffectState>(
      /*icon=*/&kVideoConferenceNoiseCancellationOnIcon,
      /*label_text=*/
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_INPUT_NOISE_CANCELLATION),
      /*accessible_name_id=*/
      IDS_ASH_STATUS_TRAY_AUDIO_INPUT_NOISE_CANCELLATION,
      /*button_callback=*/
      base::BindRepeating(&AudioEffectsController::OnEffectControlActivated,
                          weak_factory_.GetWeakPtr(),
                          /*effect_id=*/VcEffectId::kNoiseCancellation,
                          /*value=*/0));
  effect_state->set_disabled_icon(&kVideoConferenceNoiseCancellationOffIcon);
  effect->AddState(std::move(effect_state));

  effect->set_dependency_flags(VcHostedEffect::ResourceDependency::kMicrophone);
  AddEffect(std::move(effect));
}

void AudioEffectsController::AddLiveCaptionEffect() {
  std::unique_ptr<VcHostedEffect> effect = std::make_unique<VcHostedEffect>(
      /*type=*/VcEffectType::kToggle,
      /*get_state_callback=*/
      base::BindRepeating(&AudioEffectsController::GetEffectState,
                          base::Unretained(this), VcEffectId::kLiveCaption),
      /*effect_id=*/VcEffectId::kLiveCaption);

  auto effect_state = std::make_unique<VcEffectState>(
      /*icon=*/&kVideoConferenceLiveCaptionOnIcon,
      /*label_text=*/
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LIVE_CAPTION),
      /*accessible_name_id=*/
      IDS_ASH_STATUS_TRAY_LIVE_CAPTION,
      /*button_callback=*/
      base::BindRepeating(&AudioEffectsController::OnEffectControlActivated,
                          weak_factory_.GetWeakPtr(),
                          /*effect_id=*/VcEffectId::kLiveCaption,
                          /*value=*/0));
  effect_state->set_disabled_icon(&kVideoConferenceLiveCaptionOffIcon);

  effect->AddState(std::move(effect_state));
  AddEffect(std::move(effect));
}

}  // namespace ash
