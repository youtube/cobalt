// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_font_size_adjust_interpolation_type.h"

#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"

namespace blink {

class CSSFontSizeAdjustNonInterpolableValue : public NonInterpolableValue {
 public:
  ~CSSFontSizeAdjustNonInterpolableValue() override = default;

  static scoped_refptr<CSSFontSizeAdjustNonInterpolableValue> Create(
      FontSizeAdjust::Metric metric) {
    return base::AdoptRef(new CSSFontSizeAdjustNonInterpolableValue(metric));
  }

  FontSizeAdjust::Metric Metric() const { return metric_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  explicit CSSFontSizeAdjustNonInterpolableValue(FontSizeAdjust::Metric metric)
      : metric_(metric) {}

  FontSizeAdjust::Metric metric_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSFontSizeAdjustNonInterpolableValue);
template <>
struct DowncastTraits<CSSFontSizeAdjustNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() ==
           CSSFontSizeAdjustNonInterpolableValue::static_type_;
  }
};

namespace {

class InheritedFontSizeAdjustChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedFontSizeAdjustChecker(FontSizeAdjust font_size_adjust)
      : font_size_adjust_(font_size_adjust) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    return font_size_adjust_ == state.ParentStyle()->FontSizeAdjust();
  }

  const FontSizeAdjust font_size_adjust_;
};

InterpolationValue CreateFontSizeAdjustValue(FontSizeAdjust font_size_adjust) {
  if (!font_size_adjust) {
    return nullptr;
  }

  return InterpolationValue(
      std::make_unique<InterpolableNumber>(font_size_adjust.Value()),
      CSSFontSizeAdjustNonInterpolableValue::Create(
          font_size_adjust.GetMetric()));
}

}  // namespace

InterpolationValue CSSFontSizeAdjustInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  return InterpolationValue(underlying.interpolable_value->CloneAndZero(),
                            underlying.non_interpolable_value);
}

InterpolationValue CSSFontSizeAdjustInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversion_checkers) const {
  return CreateFontSizeAdjustValue(FontBuilder::InitialSizeAdjust());
}

InterpolationValue CSSFontSizeAdjustInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle()) {
    return nullptr;
  }

  FontSizeAdjust inherited_font_size_adjust =
      state.ParentStyle()->FontSizeAdjust();
  conversion_checkers.push_back(
      std::make_unique<InheritedFontSizeAdjustChecker>(
          inherited_font_size_adjust));
  return CreateFontSizeAdjustValue(inherited_font_size_adjust);
}

InterpolationValue CSSFontSizeAdjustInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState* state,
    ConversionCheckers& conversion_checkers) const {
  return CreateFontSizeAdjustValue(
      StyleBuilderConverterBase::ConvertFontSizeAdjust(*state, value));
}

PairwiseInterpolationValue
CSSFontSizeAdjustInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  const FontSizeAdjust::Metric& start_metric =
      To<CSSFontSizeAdjustNonInterpolableValue>(*start.non_interpolable_value)
          .Metric();
  const FontSizeAdjust::Metric& end_metric =
      To<CSSFontSizeAdjustNonInterpolableValue>(*end.non_interpolable_value)
          .Metric();
  if (start_metric != end_metric) {
    return nullptr;
  }
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

InterpolationValue
CSSFontSizeAdjustInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  if (!style.HasFontSizeAdjust()) {
    return nullptr;
  }
  return CreateFontSizeAdjustValue(style.FontSizeAdjust());
}

void CSSFontSizeAdjustInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  const FontSizeAdjust::Metric& underlying_metric =
      To<CSSFontSizeAdjustNonInterpolableValue>(
          *underlying_value_owner.Value().non_interpolable_value)
          .Metric();
  const FontSizeAdjust::Metric& metric =
      To<CSSFontSizeAdjustNonInterpolableValue>(*value.non_interpolable_value)
          .Metric();
  if (underlying_metric == metric) {
    underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
        underlying_fraction, *value.interpolable_value);
  } else {
    underlying_value_owner.Set(*this, value);
  }
}

void CSSFontSizeAdjustInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  state.GetFontBuilder().SetSizeAdjust(FontSizeAdjust(
      ClampTo<float>(To<InterpolableNumber>(interpolable_value).Value(), 0),
      To<CSSFontSizeAdjustNonInterpolableValue>(*non_interpolable_value)
          .Metric()));
}

}  // namespace blink
