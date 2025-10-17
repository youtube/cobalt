// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_font_palette_interpolation_type.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/animation/interpolable_font_palette.h"
#include "third_party/blink/renderer/core/animation/length_units_checker.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_palette.h"

namespace blink {

class InheritedFontPaletteChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedFontPaletteChecker(
      scoped_refptr<const FontPalette> font_palette)
      : font_palette_(font_palette) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    return ValuesEquivalent(font_palette_.get(),
                            state.ParentStyle()->GetFontPalette());
  }

  scoped_refptr<const FontPalette> font_palette_;
};

InterpolationValue CSSFontPaletteInterpolationType::ConvertFontPalette(
    scoped_refptr<const FontPalette> font_palette) {
  if (!font_palette) {
    return InterpolationValue(
        InterpolableFontPalette::Create(FontPalette::Create()));
  }
  return InterpolationValue(InterpolableFontPalette::Create(font_palette));
}

InterpolationValue CSSFontPaletteInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers&) const {
  return InterpolationValue(underlying.interpolable_value->CloneAndZero(),
                            underlying.non_interpolable_value);
}

InterpolationValue CSSFontPaletteInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  return ConvertFontPalette(FontPalette::Create());
}

InterpolationValue CSSFontPaletteInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  DCHECK(state.ParentStyle());
  scoped_refptr<const FontPalette> inherited_font_palette =
      state.ParentStyle()->GetFontPalette();
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedFontPaletteChecker>(
          inherited_font_palette));
  return ConvertFontPalette(inherited_font_palette);
}

InterpolationValue CSSFontPaletteInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  // TODO(crbug.com/415626999): Create a TreeCountingChecker for sibling-index()
  // and sibling-count() if necessary.
  if (auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value)) {
    CSSPrimitiveValue::LengthTypeFlags types;
    primitive_value->AccumulateLengthUnitTypes(types);
    if (InterpolationType::ConversionChecker* length_units_checker =
            LengthUnitsChecker::MaybeCreate(types, state)) {
      conversion_checkers.push_back(length_units_checker);
    }
  }
  return ConvertFontPalette(StyleBuilderConverterBase::ConvertFontPalette(
      state.CssToLengthConversionData(), value));
}

InterpolationValue
CSSFontPaletteInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  const FontPalette* font_palette = style.GetFontPalette();
  return ConvertFontPalette(font_palette);
}

void CSSFontPaletteInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const InterpolableFontPalette& interpolable_font_palette =
      To<InterpolableFontPalette>(interpolable_value);

  scoped_refptr<const FontPalette> font_palette =
      interpolable_font_palette.GetFontPalette();

  state.GetFontBuilder().SetFontPalette(font_palette);
}

}  // namespace blink
