// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_AUDIO_EFFECTS_CONTROLLER_H_
#define ASH_SYSTEM_AUDIO_AUDIO_EFFECTS_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_delegate.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

enum class VcEffectId;

class ASH_EXPORT AudioEffectsController
    : public CrasAudioHandler::AudioObserver,
      public SessionObserver,
      public VcEffectsDelegate {
 public:
  AudioEffectsController();

  AudioEffectsController(const AudioEffectsController&) = delete;
  AudioEffectsController& operator=(const AudioEffectsController&) = delete;

  ~AudioEffectsController() override;

  // Returns whether `effect_id` is supported.
  bool IsEffectSupported(VcEffectId effect_id);

  // VcEffectsDelegate:
  absl::optional<int> GetEffectState(VcEffectId effect_id) override;
  void OnEffectControlActivated(VcEffectId effect_id,
                                absl::optional<int> state) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

 private:
  // CrasAudioHandler::AudioObserver:
  void OnActiveInputNodeChanged() override;

  // Construct effect for noise cancellation.
  void AddNoiseCancellationEffect();

  // Construct effect for live caption.
  void AddLiveCaptionEffect();

  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};

  // Indicates if noise cancellation is supported for the current input device.
  bool noise_cancellation_supported_ = false;

  base::WeakPtrFactory<AudioEffectsController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_AUDIO_EFFECTS_CONTROLLER_H_