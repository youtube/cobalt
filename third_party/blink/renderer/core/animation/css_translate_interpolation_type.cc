// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_translate_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/transforms/translate_transform_operation.h"

namespace blink {

namespace {

InterpolationValue CreateNoneValue() {
  return InterpolationValue(std::make_unique<InterpolableList>(0));
}

bool IsNoneValue(const InterpolationValue& value) {
  return To<InterpolableList>(*value.interpolable_value).length() == 0;
}

class InheritedTranslateChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedTranslateChecker(
      scoped_refptr<TranslateTransformOperation> inherited_translate)
      : inherited_translate_(std::move(inherited_translate)) {}
  ~InheritedTranslateChecker() override = default;


  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    const TransformOperation* inherited_translate =
        state.ParentStyle()->Translate();
    if (inherited_translate_ == inherited_translate)
      return true;
    if (!inherited_translate_ || !inherited_translate)
      return false;
    return *inherited_translate_ == *inherited_translate;
  }

 private:
  scoped_refptr<TransformOperation> inherited_translate_;
};

enum TranslateComponentIndex : unsigned {
  kTranslateX,
  kTranslateY,
  kTranslateZ,
  kTranslateComponentIndexCount,
};

std::unique_ptr<InterpolableValue> CreateTranslateIdentity() {
  auto result =
      std::make_unique<InterpolableList>(kTranslateComponentIndexCount);
  result->Set(kTranslateX, InterpolableLength::CreateNeutral());
  result->Set(kTranslateY, InterpolableLength::CreateNeutral());
  result->Set(kTranslateZ, InterpolableLength::CreateNeutral());
  return std::move(result);
}

InterpolationValue ConvertTranslateOperation(
    const TranslateTransformOperation* translate,
    double zoom) {
  if (!translate)
    return CreateNoneValue();

  auto result =
      std::make_unique<InterpolableList>(kTranslateComponentIndexCount);
  result->Set(kTranslateX,
              InterpolableLength::MaybeConvertLength(translate->X(), zoom));
  result->Set(kTranslateY,
              InterpolableLength::MaybeConvertLength(translate->Y(), zoom));
  result->Set(kTranslateZ, InterpolableLength::MaybeConvertLength(
                               Length::Fixed(translate->Z()), zoom));
  return InterpolationValue(std::move(result));
}

}  // namespace

InterpolationValue CSSTranslateInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers&) const {
  return InterpolationValue(CreateTranslateIdentity());
}

InterpolationValue CSSTranslateInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return CreateNoneValue();
}

InterpolationValue CSSTranslateInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  TranslateTransformOperation* inherited_translate =
      state.ParentStyle()->Translate();
  conversion_checkers.push_back(
      std::make_unique<InheritedTranslateChecker>(inherited_translate));
  return ConvertTranslateOperation(inherited_translate,
                                   state.ParentStyle()->EffectiveZoom());
}

InterpolationValue CSSTranslateInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.IsBaseValueList()) {
    return CreateNoneValue();
  }

  const auto& list = To<CSSValueList>(value);
  if (list.length() < 1 || list.length() > 3)
    return nullptr;

  auto result =
      std::make_unique<InterpolableList>(kTranslateComponentIndexCount);
  for (wtf_size_t i = 0; i < kTranslateComponentIndexCount; i++) {
    InterpolationValue component = nullptr;
    if (i < list.length()) {
      component = InterpolationValue(
          InterpolableLength::MaybeConvertCSSValue(list.Item(i)));
      if (!component)
        return nullptr;
    } else {
      component = InterpolationValue(InterpolableLength::CreateNeutral());
    }
    result->Set(i, std::move(component.interpolable_value));
  }
  return InterpolationValue(std::move(result));
}

PairwiseInterpolationValue CSSTranslateInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  size_t start_list_length =
      To<InterpolableList>(*start.interpolable_value).length();
  size_t end_list_length =
      To<InterpolableList>(*end.interpolable_value).length();
  if (start_list_length < end_list_length)
    start.interpolable_value = CreateTranslateIdentity();
  else if (end_list_length < start_list_length)
    end.interpolable_value = CreateTranslateIdentity();

  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value));
}

InterpolationValue
CSSTranslateInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return ConvertTranslateOperation(style.Translate(), style.EffectiveZoom());
}

void CSSTranslateInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  if (IsNoneValue(value)) {
    return;
  }

  if (IsNoneValue(underlying_value_owner.MutableValue())) {
    underlying_value_owner.MutableValue().interpolable_value =
        CreateTranslateIdentity();
  }

  return CSSInterpolationType::Composite(underlying_value_owner,
                                         underlying_fraction, value,
                                         interpolation_fraction);
}

void CSSTranslateInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  const auto& list = To<InterpolableList>(interpolable_value);
  if (list.length() == 0) {
    state.StyleBuilder().SetTranslate(nullptr);
    return;
  }
  const CSSToLengthConversionData& conversion_data =
      state.CssToLengthConversionData();
  Length x = To<InterpolableLength>(*list.Get(kTranslateX))
                 .CreateLength(conversion_data, Length::ValueRange::kAll);
  Length y = To<InterpolableLength>(*list.Get(kTranslateY))
                 .CreateLength(conversion_data, Length::ValueRange::kAll);
  float z = To<InterpolableLength>(*list.Get(kTranslateZ))
                .CreateLength(conversion_data, Length::ValueRange::kAll)
                .Pixels();

  scoped_refptr<TranslateTransformOperation> result =
      TranslateTransformOperation::Create(x, y, z,
                                          TransformOperation::kTranslate3D);
  state.StyleBuilder().SetTranslate(std::move(result));
}

}  // namespace blink
