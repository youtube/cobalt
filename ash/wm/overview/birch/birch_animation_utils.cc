// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_animation_utils.h"

#include "ash/utility/lottie_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/view_utils.h"

namespace ash::birch_animation_utils {

namespace {

constexpr char kLoadingAnimationRestartMarker[] =
    "_CrOS_Marker_CycleRestart_Line";

base::TimeDelta GetCycleRestartTimestamp(const cc::SkottieWrapper& skottie,
                                         int animation_id) {
  CHECK(skottie.is_valid());
  CHECK_EQ(skottie.GetAllMarkers().size(), 1u);

  auto marker = skottie.GetAllMarkers().begin();
  CHECK_EQ(marker->name, kLoadingAnimationRestartMarker);

  // `marker.begin_time` is a normalized timestamp in range [0, 1), where 1
  // is the animation's cycle duration.
  return base::TimeDelta(base::Seconds(skottie.duration()) *
                         marker->begin_time);
}

}  // namespace

std::unique_ptr<lottie::Animation> GetLottieAnimationData(int animation_id) {
  std::optional<std::vector<uint8_t>> lottie_data =
      ui::ResourceBundle::GetSharedInstance().GetLottieData(animation_id);
  CHECK(lottie_data.has_value());

  std::unique_ptr<lottie::Animation> animation =
      std::make_unique<lottie::Animation>(
          cc::SkottieWrapper::UnsafeCreateSerializable(lottie_data.value()));

  return animation;
}

std::optional<lottie::Animation::PlaybackConfig> GetLottiePlaybackConfig(
    const cc::SkottieWrapper& skottie,
    int animation_id) {
  auto animation_duration = base::Seconds(skottie.duration());
  return lottie::Animation::PlaybackConfig(
      {{base::TimeDelta(), animation_duration},
       {GetCycleRestartTimestamp(skottie, animation_id), animation_duration}},
      /*initial_offset=*/base::TimeDelta(),
      /*initial_completed_cycles=*/0, lottie::Animation::Style::kLoop);
}

}  // namespace ash::birch_animation_utils
