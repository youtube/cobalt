// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_intrinsic_length_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_intrinsic_length.h"

namespace blink {

class CSSIntrinsicLengthNonInterpolableValue final
    : public NonInterpolableValue {
 public:
  ~CSSIntrinsicLengthNonInterpolableValue() final = default;

  enum EType { kNone, kAutoAndLength, kLength };

  static scoped_refptr<CSSIntrinsicLengthNonInterpolableValue> Create(
      absl::optional<StyleIntrinsicLength> intrinsic_dimension) {
    EType type = kLength;
    if (!intrinsic_dimension)
      type = kNone;
    else if (intrinsic_dimension->HasAuto())
      type = kAutoAndLength;
    return base::AdoptRef(new CSSIntrinsicLengthNonInterpolableValue(type));
  }

  bool IsNone() const { return type_ == kNone; }
  bool HasAuto() const { return type_ == kAutoAndLength; }

  bool IsCompatibleWith(
      const CSSIntrinsicLengthNonInterpolableValue& other) const {
    if (IsNone() || other.IsNone() || (HasAuto() != other.HasAuto()))
      return false;
    return true;
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  explicit CSSIntrinsicLengthNonInterpolableValue(EType type) : type_(type) {}

  EType type_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSIntrinsicLengthNonInterpolableValue);
template <>
struct DowncastTraits<CSSIntrinsicLengthNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() ==
           CSSIntrinsicLengthNonInterpolableValue::static_type_;
  }
};

class InheritedIntrinsicDimensionChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedIntrinsicDimensionChecker(
      bool is_width,
      absl::optional<StyleIntrinsicLength> intrinsic_dimension)
      : is_width_(is_width), intrinsic_dimension_(intrinsic_dimension) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    if (is_width_) {
      return state.ParentStyle()->ContainIntrinsicWidth() ==
             intrinsic_dimension_;
    }
    return state.ParentStyle()->ContainIntrinsicHeight() ==
           intrinsic_dimension_;
  }

  bool is_width_;
  const absl::optional<StyleIntrinsicLength> intrinsic_dimension_;
};

std::unique_ptr<InterpolableValue>
CSSIntrinsicLengthInterpolationType::CreateInterpolableIntrinsicDimension(
    const absl::optional<StyleIntrinsicLength>& intrinsic_dimension) {
  if (!intrinsic_dimension)
    return nullptr;

  DCHECK(intrinsic_dimension->GetLength().IsFixed());
  return InterpolableLength::CreatePixels(
      intrinsic_dimension->GetLength().Value());
}

PairwiseInterpolationValue
CSSIntrinsicLengthInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  if (!To<CSSIntrinsicLengthNonInterpolableValue>(*start.non_interpolable_value)
           .IsCompatibleWith(To<CSSIntrinsicLengthNonInterpolableValue>(
               *end.non_interpolable_value))) {
    return nullptr;
  }
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

absl::optional<StyleIntrinsicLength>
CSSIntrinsicLengthInterpolationType::GetIntrinsicDimension(
    const ComputedStyle& style) const {
  return CssProperty().PropertyID() == CSSPropertyID::kContainIntrinsicWidth
             ? style.ContainIntrinsicWidth()
             : style.ContainIntrinsicHeight();
}

void CSSIntrinsicLengthInterpolationType::SetIntrinsicDimension(
    ComputedStyleBuilder& builder,
    const absl::optional<StyleIntrinsicLength>& dimension) const {
  if (CssProperty().PropertyID() == CSSPropertyID::kContainIntrinsicWidth)
    builder.SetContainIntrinsicWidth(dimension);
  else
    builder.SetContainIntrinsicHeight(dimension);
}

InterpolationValue CSSIntrinsicLengthInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  return InterpolationValue(underlying.interpolable_value->CloneAndZero(),
                            underlying.non_interpolable_value);
}

InterpolationValue CSSIntrinsicLengthInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  absl::optional<StyleIntrinsicLength> initial_dimension =
      GetIntrinsicDimension(
          state.GetDocument().GetStyleResolver().InitialStyle());
  return InterpolationValue(
      CreateInterpolableIntrinsicDimension(initial_dimension),
      CSSIntrinsicLengthNonInterpolableValue::Create(initial_dimension));
}

InterpolationValue CSSIntrinsicLengthInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;

  absl::optional<StyleIntrinsicLength> inherited_intrinsic_dimension =
      GetIntrinsicDimension(*state.ParentStyle());
  conversion_checkers.push_back(
      std::make_unique<InheritedIntrinsicDimensionChecker>(
          CssProperty().PropertyID() == CSSPropertyID::kContainIntrinsicWidth,
          inherited_intrinsic_dimension));
  if (!inherited_intrinsic_dimension)
    return nullptr;

  return InterpolationValue(
      CreateInterpolableIntrinsicDimension(inherited_intrinsic_dimension),
      CSSIntrinsicLengthNonInterpolableValue::Create(
          inherited_intrinsic_dimension));
}

InterpolationValue CSSIntrinsicLengthInterpolationType::
    MaybeConvertStandardPropertyUnderlyingValue(
        const ComputedStyle& style) const {
  absl::optional<StyleIntrinsicLength> dimension = GetIntrinsicDimension(style);
  return InterpolationValue(
      CreateInterpolableIntrinsicDimension(dimension),
      CSSIntrinsicLengthNonInterpolableValue::Create(dimension));
}

InterpolationValue CSSIntrinsicLengthInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState* state,
    ConversionCheckers&) const {
  absl::optional<StyleIntrinsicLength> dimension =
      StyleBuilderConverter::ConvertIntrinsicDimension(*state, value);
  return InterpolationValue(
      CreateInterpolableIntrinsicDimension(dimension),
      CSSIntrinsicLengthNonInterpolableValue::Create(dimension));
}

void CSSIntrinsicLengthInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const auto& interpolable = To<InterpolableLength>(interpolable_value);
  const auto* non_interpolable =
      To<CSSIntrinsicLengthNonInterpolableValue>(non_interpolable_value);
  if (non_interpolable->IsNone()) {
    SetIntrinsicDimension(state.StyleBuilder(), absl::nullopt);
  } else {
    SetIntrinsicDimension(
        state.StyleBuilder(),
        StyleIntrinsicLength(
            non_interpolable->HasAuto(),
            interpolable.CreateLength(state.CssToLengthConversionData(),
                                      Length::ValueRange::kNonNegative)));
  }
}
void CSSIntrinsicLengthInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
      underlying_fraction, *value.interpolable_value);
}

}  // namespace blink
