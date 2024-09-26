// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_grid_track_repeater.h"

#include <memory>
#include "third_party/blink/renderer/core/animation/interpolable_grid_track_size.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

InterpolableGridTrackRepeater::InterpolableGridTrackRepeater(
    std::unique_ptr<InterpolableList> values,
    const NGGridTrackRepeater& repeater)
    : values_(std::move(values)), repeater_(repeater) {
  DCHECK(values_);
}

// static
std::unique_ptr<InterpolableGridTrackRepeater>
InterpolableGridTrackRepeater::Create(
    const NGGridTrackRepeater& repeater,
    const Vector<GridTrackSize, 1>& repeater_track_sizes,
    float zoom) {
  DCHECK_EQ(repeater_track_sizes.size(), repeater.repeat_size);

  std::unique_ptr<InterpolableList> values =
      std::make_unique<InterpolableList>(repeater_track_sizes.size());
  for (wtf_size_t i = 0; i < repeater_track_sizes.size(); ++i) {
    std::unique_ptr<InterpolableGridTrackSize> result =
        InterpolableGridTrackSize::Create(repeater_track_sizes[i], zoom);
    DCHECK(result);
    values->Set(i, std::move(result));
  }
  return std::make_unique<InterpolableGridTrackRepeater>(std::move(values),
                                                         repeater);
}

Vector<GridTrackSize, 1> InterpolableGridTrackRepeater::CreateTrackSizes(
    const CSSToLengthConversionData& conversion_data) const {
  DCHECK_EQ(values_->length(), repeater_.repeat_size);

  Vector<GridTrackSize, 1> track_sizes;
  track_sizes.ReserveInitialCapacity(values_->length());
  for (wtf_size_t i = 0; i < values_->length(); ++i) {
    const InterpolableGridTrackSize& interpolable_track_size =
        To<InterpolableGridTrackSize>(*values_->Get(i));
    track_sizes.push_back(
        interpolable_track_size.CreateTrackSize(conversion_data));
  }
  return track_sizes;
}

InterpolableGridTrackRepeater* InterpolableGridTrackRepeater::RawClone() const {
  std::unique_ptr<InterpolableList> values(
      DynamicTo<InterpolableList>(values_->Clone().release()));
  return new InterpolableGridTrackRepeater(std::move(values), repeater_);
}

InterpolableGridTrackRepeater* InterpolableGridTrackRepeater::RawCloneAndZero()
    const {
  std::unique_ptr<InterpolableList> values(
      DynamicTo<InterpolableList>(values_->CloneAndZero().release()));
  return new InterpolableGridTrackRepeater(std::move(values), repeater_);
}

bool InterpolableGridTrackRepeater::Equals(
    const InterpolableValue& other) const {
  return values_->Equals(*(To<InterpolableGridTrackRepeater>(other).values_));
}

void InterpolableGridTrackRepeater::Scale(double scale) {
  values_->Scale(scale);
}

void InterpolableGridTrackRepeater::Add(const InterpolableValue& other) {
  DCHECK(IsCompatibleWith(other));
  values_->Add(*(To<InterpolableGridTrackRepeater>(other).values_));
}

bool InterpolableGridTrackRepeater::IsCompatibleWith(
    const InterpolableValue& other) const {
  const InterpolableGridTrackRepeater& other_interpolable_grid_track_repeater =
      To<InterpolableGridTrackRepeater>(other);
  return values_->length() ==
             other_interpolable_grid_track_repeater.values_->length() &&
         repeater_ == other_interpolable_grid_track_repeater.repeater_;
}

void InterpolableGridTrackRepeater::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  const InterpolableGridTrackRepeater& other_interpolable_grid_track_repeater =
      To<InterpolableGridTrackRepeater>(other);
  DCHECK_EQ(values_->length(),
            other_interpolable_grid_track_repeater.values_->length());
  DCHECK_EQ(repeater_, other_interpolable_grid_track_repeater.repeater_);
  values_->AssertCanInterpolateWith(
      *other_interpolable_grid_track_repeater.values_);
}

void InterpolableGridTrackRepeater::Interpolate(
    const InterpolableValue& to,
    const double progress,
    InterpolableValue& result) const {
  const InterpolableGridTrackRepeater& grid_track_repeater_to =
      To<InterpolableGridTrackRepeater>(to);
  InterpolableGridTrackRepeater& grid_track_repeater_result =
      To<InterpolableGridTrackRepeater>(result);
  values_->Interpolate(*grid_track_repeater_to.values_, progress,
                       *grid_track_repeater_result.values_);
}

}  // namespace blink
