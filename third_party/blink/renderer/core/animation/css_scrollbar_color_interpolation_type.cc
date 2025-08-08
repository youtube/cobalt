// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_scrollbar_color_interpolation_type.h"

#include <memory>
#include <tuple>
#include <utility>

#include "third_party/blink/renderer/core/animation/interpolable_scrollbar_color.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

absl::optional<StyleColor> ColorFromKeyword(CSSValueID css_value_id) {
  // TODO(kevers): handle currentcolor etc.
  if (!StyleColor::IsColorKeyword(css_value_id)) {
    return absl::nullopt;
  }

  Color color = StyleColor::ColorFromKeyword(css_value_id,
                                             mojom::blink::ColorScheme::kLight);
  return (StyleColor(color));
}

absl::optional<StyleColor> MaybeResolveColor(const CSSValue& value) {
  if (auto* color_value = DynamicTo<cssvalue::CSSColor>(value)) {
    return StyleColor(color_value->Value());
  } else if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    return ColorFromKeyword(identifier_value->GetValueID());
  }
  // TODO(kevers): Handle unsupported color representations, i.e.
  // CSSColorMixValue.
  return absl::nullopt;
}

}  // namespace

class CSSScrollbarColorNonInterpolableValue final
    : public NonInterpolableValue {
 public:
  ~CSSScrollbarColorNonInterpolableValue() final = default;

  static scoped_refptr<CSSScrollbarColorNonInterpolableValue> Create(
      absl::optional<StyleScrollbarColor> scrollbar_color) {
    return base::AdoptRef(
        new CSSScrollbarColorNonInterpolableValue(scrollbar_color.has_value()));
  }

  bool HasValue() const { return has_value_; }

  bool IsCompatibleWith(
      const CSSScrollbarColorNonInterpolableValue& other) const {
    if (!HasValue() || HasValue() != other.HasValue()) {
      return false;
    }
    return true;
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  explicit CSSScrollbarColorNonInterpolableValue(bool has_value)
      : has_value_(has_value) {}

  bool has_value_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSScrollbarColorNonInterpolableValue);
template <>
struct DowncastTraits<CSSScrollbarColorNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() ==
           CSSScrollbarColorNonInterpolableValue::static_type_;
  }
};

class InheritedScrollbarColorChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedScrollbarColorChecker(
      absl::optional<StyleScrollbarColor> scrollbar_color)
      : scrollbar_color_(scrollbar_color) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return scrollbar_color_ == state.ParentStyle()->ScrollbarColor();
  }

  absl::optional<StyleScrollbarColor> scrollbar_color_;
};

InterpolationValue CSSScrollbarColorInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  return InterpolationValue(underlying.interpolable_value->CloneAndZero(),
                            underlying.non_interpolable_value);
}

InterpolationValue CSSScrollbarColorInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  absl::optional<StyleScrollbarColor> initial_scrollbar_color =
      state.GetDocument().GetStyleResolver().InitialStyle().ScrollbarColor();
  return InterpolationValue(
      CreateScrollbarColorValue(initial_scrollbar_color),
      CSSScrollbarColorNonInterpolableValue::Create(initial_scrollbar_color));
}

InterpolationValue CSSScrollbarColorInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle()) {
    return nullptr;
  }

  absl::optional<StyleScrollbarColor> inherited_scrollbar_color =
      state.ParentStyle()->ScrollbarColor();
  conversion_checkers.push_back(
      std::make_unique<InheritedScrollbarColorChecker>(
          inherited_scrollbar_color));

  if (!inherited_scrollbar_color.has_value()) {
    return nullptr;
  }

  return InterpolationValue(
      CreateScrollbarColorValue(inherited_scrollbar_color),
      CSSScrollbarColorNonInterpolableValue::Create(inherited_scrollbar_color));
}

InterpolationValue CSSScrollbarColorInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState* state,
    ConversionCheckers& conversion_checkers) const {
  // https://drafts.csswg.org/css-scrollbars/#scrollbar-color
  // scrollbar-color: auto | <color>{2}
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    if (identifier_value->GetValueID() == CSSValueID::kAuto) {
      // Fallback to discrete interpolation. The thumb and track colors depend
      // on the native theme.
      return nullptr;
    }
  }

  const CSSValueList& list = To<CSSValueList>(value);
  DCHECK_EQ(list.length(), 2u);
  absl::optional<StyleColor> thumb_color = MaybeResolveColor(list.First());
  absl::optional<StyleColor> track_color = MaybeResolveColor(list.Last());
  if (!thumb_color || !track_color) {
    // Fallback to discrete if unable to resolve the thumb or track color.
    return nullptr;
  }

  StyleScrollbarColor scrollbar_color(thumb_color.value(), track_color.value());

  return InterpolationValue(InterpolableScrollbarColor::Create(scrollbar_color),
                            CSSScrollbarColorNonInterpolableValue::Create(
                                absl::make_optional(scrollbar_color)));
}

PairwiseInterpolationValue
CSSScrollbarColorInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  if (!To<CSSScrollbarColorNonInterpolableValue>(*start.non_interpolable_value)
           .IsCompatibleWith(To<CSSScrollbarColorNonInterpolableValue>(
               *end.non_interpolable_value))) {
    return nullptr;
  }

  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

InterpolationValue
CSSScrollbarColorInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return InterpolationValue(
      CreateScrollbarColorValue(style.ScrollbarColor()),
      CSSScrollbarColorNonInterpolableValue::Create(style.ScrollbarColor()));
}

void CSSScrollbarColorInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double) const {
  if (!To<CSSScrollbarColorNonInterpolableValue>(
           *underlying_value_owner.Value().non_interpolable_value)
           .IsCompatibleWith(To<CSSScrollbarColorNonInterpolableValue>(
               *value.non_interpolable_value))) {
    underlying_value_owner.Set(*this, value);
  }

  auto& underlying = To<InterpolableScrollbarColor>(
      *underlying_value_owner.MutableValue().interpolable_value);
  const auto& other = To<InterpolableScrollbarColor>(*value.interpolable_value);
  underlying.Composite(other, underlying_fraction);
}

void CSSScrollbarColorInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  const auto& interpolable_scrollbar_color =
      To<InterpolableScrollbarColor>(interpolable_value);
  StyleScrollbarColor scrollbar_color =
      interpolable_scrollbar_color.GetScrollbarColor(state);
  state.StyleBuilder().SetScrollbarColor(scrollbar_color);
}

std::unique_ptr<InterpolableScrollbarColor>
CSSScrollbarColorInterpolationType::CreateScrollbarColorValue(
    absl::optional<StyleScrollbarColor> scrollbar_color) const {
  if (!scrollbar_color.has_value()) {
    return nullptr;
  }
  return InterpolableScrollbarColor::Create(scrollbar_color.value());
}

}  // namespace blink
