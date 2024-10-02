// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_grid_length.h"

#include <memory>
#include "third_party/blink/renderer/core/animation/interpolable_length.h"

namespace blink {

namespace {

InterpolableGridLength::InterpolableGridLengthType
GetInterpolableGridLengthType(const Length& length) {
  switch (length.GetType()) {
    case Length::kAuto:
      return InterpolableGridLength::kAuto;
    case Length::kMinContent:
      return InterpolableGridLength::kMinContent;
    case Length::kMaxContent:
      return InterpolableGridLength::kMaxContent;
    default:
      return InterpolableGridLength::kLength;
  }
}

Length CreateContentSizedLength(
    const InterpolableGridLength::InterpolableGridLengthType& type) {
  switch (type) {
    case InterpolableGridLength::kAuto:
      return Length(Length::kAuto);
    case InterpolableGridLength::kMinContent:
      return Length(Length::kMinContent);
    case InterpolableGridLength::kMaxContent:
      return Length(Length::kMaxContent);
    default:
      NOTREACHED();
      return Length(Length::kFixed);
  }
}
}  // namespace

InterpolableGridLength::InterpolableGridLength(
    std::unique_ptr<InterpolableValue> value,
    InterpolableGridLengthType type)
    : value_(std::move(value)), type_(type) {
  DCHECK(value_ || IsContentSized());
}

// static
std::unique_ptr<InterpolableGridLength> InterpolableGridLength::Create(
    const GridLength& grid_length,
    float zoom) {
  std::unique_ptr<InterpolableValue> value;
  InterpolableGridLengthType type;
  Length length(Length::kAuto);
  if (grid_length.IsLength()) {
    length = grid_length.length();
    value = grid_length.IsContentSized()
                ? nullptr
                : InterpolableLength::MaybeConvertLength(length, zoom);
    type = GetInterpolableGridLengthType(length);
  } else {
    value = std::make_unique<InterpolableNumber>(grid_length.Flex());
    type = kFlex;
  }
  return std::make_unique<InterpolableGridLength>(std::move(value), type);
}

GridLength InterpolableGridLength::CreateGridLength(
    const CSSToLengthConversionData& conversion_data) const {
  if (IsContentSized())
    return GridLength(CreateContentSizedLength(type_));

  DCHECK(value_);
  if (type_ == kFlex)
    return GridLength(To<InterpolableNumber>(*value_).Value());
  Length length = To<InterpolableLength>(*value_).CreateLength(
      conversion_data, Length::ValueRange::kNonNegative);
  return GridLength(length);
}

bool InterpolableGridLength::IsContentSized() const {
  return type_ == kAuto || type_ == kMinContent || type_ == kMaxContent;
}

bool InterpolableGridLength::IsCompatibleWith(
    const InterpolableGridLength& other) const {
  return !IsContentSized() && !other.IsContentSized() && (type_ == other.type_);
}

InterpolableGridLength* InterpolableGridLength::RawClone() const {
  return new InterpolableGridLength(value_ ? value_->Clone() : nullptr, type_);
}

InterpolableGridLength* InterpolableGridLength::RawCloneAndZero() const {
  return new InterpolableGridLength(value_ ? value_->CloneAndZero() : nullptr,
                                    type_);
}

bool InterpolableGridLength::Equals(const InterpolableValue& other) const {
  // TODO (ansollan): Check for the equality of |value_| when Equals() is
  // implemented in |InterpolableLength|.
  return type_ == To<InterpolableGridLength>(other).type_;
}

void InterpolableGridLength::Scale(double scale) {
  // We can scale a value only if this is either an |InterpolableNumber| or
  // |InterpolableLength|.
  if (!IsContentSized()) {
    DCHECK(value_);
    value_->Scale(scale);
  }
}

void InterpolableGridLength::Add(const InterpolableValue& other) {
  const InterpolableGridLength& other_interpolable_grid_length =
      To<InterpolableGridLength>(other);

  // We can add two values only if their types match and they aren't content
  // sized. Otherwise, the value and type are replaced.
  if (IsCompatibleWith(other_interpolable_grid_length)) {
    DCHECK(value_ && other_interpolable_grid_length.value_);
    value_->Add(*other_interpolable_grid_length.value_);
  } else {
    type_ = other_interpolable_grid_length.type_;
    value_ = other_interpolable_grid_length.value_
                 ? other_interpolable_grid_length.value_->Clone()
                 : nullptr;
  }
}

void InterpolableGridLength::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  const InterpolableGridLength& other_interpolable_grid_length =
      To<InterpolableGridLength>(other);

  // If the types for both interpolable values are equal and are either length
  // or flex, we can directly call |AssertCanInterpolateWith| on |value_|, as
  // it should either be |InterpolableLength| or |InterpolableNumber|.
  // Otherwise, at least one of the types is content sized or they aren't equal.
  if ((type_ == kLength && other_interpolable_grid_length.type_ == kLength) ||
      (type_ == kFlex && other_interpolable_grid_length.type_ == kFlex)) {
    DCHECK(value_ && other_interpolable_grid_length.value_);
    value_->AssertCanInterpolateWith(*other_interpolable_grid_length.value_);
  } else {
    DCHECK(!IsCompatibleWith(other_interpolable_grid_length));
  }
}

void InterpolableGridLength::Interpolate(const InterpolableValue& to,
                                         const double progress,
                                         InterpolableValue& result) const {
  const InterpolableGridLength& grid_length_to = To<InterpolableGridLength>(to);
  InterpolableGridLength& grid_length_result =
      To<InterpolableGridLength>(result);
  if (!IsCompatibleWith(grid_length_to)) {
    if (progress < 0.5) {
      grid_length_result.type_ = type_;
      grid_length_result.value_ = value_ ? value_->Clone() : nullptr;
    } else {
      grid_length_result.type_ = grid_length_to.type_;
      grid_length_result.value_ =
          grid_length_to.value_ ? grid_length_to.value_->Clone() : nullptr;
    }
    return;
  }
  value_->Interpolate(*grid_length_to.value_, progress,
                      *grid_length_result.value_);
}

}  // namespace blink
