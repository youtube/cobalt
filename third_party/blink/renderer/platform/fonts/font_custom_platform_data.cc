/*
 * Copyright (C) 2007 Apple Computer, Inc.
 * Copyright (c) 2007, 2008, 2009, Google Inc. All rights reserved.
 * Copyright (C) 2010 Company 100, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/opentype/font_format_check.h"
#include "third_party/blink/renderer/platform/fonts/opentype/font_settings.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_cpal_lookup.h"
#include "third_party/blink/renderer/platform/fonts/opentype/variable_axes_names.h"
#include "third_party/blink/renderer/platform/fonts/web_font_decoder.h"
#include "third_party/blink/renderer/platform/fonts/web_font_typeface_factory.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace {

constexpr SkFourByteTag kOpszTag = SkSetFourByteTag('o', 'p', 's', 'z');
constexpr SkFourByteTag kSlntTag = SkSetFourByteTag('s', 'l', 'n', 't');
constexpr SkFourByteTag kWdthTag = SkSetFourByteTag('w', 'd', 't', 'h');
constexpr SkFourByteTag kWghtTag = SkSetFourByteTag('w', 'g', 'h', 't');

absl::optional<SkFontParameters::Variation::Axis>
RetrieveVariationDesignParametersByTag(sk_sp<SkTypeface> base_typeface,
                                       SkFourByteTag tag) {
  int axes_count = base_typeface->getVariationDesignParameters(nullptr, 0);
  if (axes_count <= 0)
    return absl::nullopt;
  Vector<SkFontParameters::Variation::Axis> axes;
  axes.resize(axes_count);
  int axes_read =
      base_typeface->getVariationDesignParameters(axes.data(), axes_count);
  if (axes_read <= 0)
    return absl::nullopt;
  for (auto& axis : axes) {
    if (axis.tag == tag) {
      return axis;
    }
  }
  return absl::nullopt;
}

}  // namespace

namespace blink {

FontCustomPlatformData::FontCustomPlatformData(sk_sp<SkTypeface> typeface,
                                               size_t data_size)
    : base_typeface_(std::move(typeface)), data_size_(data_size) {}

FontCustomPlatformData::~FontCustomPlatformData() = default;

FontPlatformData FontCustomPlatformData::GetFontPlatformData(
    float size,
    float adjusted_specified_size,
    bool bold,
    bool italic,
    const FontSelectionRequest& selection_request,
    const FontSelectionCapabilities& selection_capabilities,
    const OpticalSizing& optical_sizing,
    TextRenderingMode text_rendering,
    const ResolvedFontFeatures& resolved_font_features,
    FontOrientation orientation,
    const FontVariationSettings* variation_settings,
    const FontPalette* palette) {
  DCHECK(base_typeface_);

  sk_sp<SkTypeface> return_typeface = base_typeface_;

  // Maximum axis count is maximum value for the OpenType USHORT,
  // which is a 16bit unsigned.
  // https://www.microsoft.com/typography/otspec/fvar.htm Variation
  // settings coming from CSS can have duplicate assignments and the
  // list can be longer than UINT16_MAX, but ignoring the length for
  // now, going with a reasonable upper limit. Deduplication is
  // handled by Skia with priority given to the last occuring
  // assignment.
  FontFormatCheck::VariableFontSubType font_sub_type =
      FontFormatCheck::ProbeVariableFont(base_typeface_);
  bool synthetic_bold = bold;
  bool synthetic_italic = italic;
  if (font_sub_type ==
          FontFormatCheck::VariableFontSubType::kVariableTrueType ||
      font_sub_type == FontFormatCheck::VariableFontSubType::kVariableCFF2) {
    Vector<SkFontArguments::VariationPosition::Coordinate, 0> variation;

    SkFontArguments::VariationPosition::Coordinate weight_coordinate = {
        kWghtTag, SkFloatToScalar(selection_capabilities.weight.clampToRange(
                      selection_request.weight))};
    absl::optional<SkFontParameters::Variation::Axis> wght_parameters =
        RetrieveVariationDesignParametersByTag(base_typeface_, kWghtTag);
    if (selection_capabilities.weight.IsRangeSetFromAuto() && wght_parameters) {
      DCHECK(RuntimeEnabledFeatures::CSSFontFaceAutoVariableRangeEnabled());
      FontSelectionRange wght_range = {
          FontSelectionValue(wght_parameters->min),
          FontSelectionValue(wght_parameters->max)};
      weight_coordinate = {
          kWghtTag,
          SkFloatToScalar(wght_range.clampToRange(selection_request.weight))};
      synthetic_bold = bold && wght_range.maximum < BoldThreshold() &&
                       selection_request.weight >= BoldThreshold();
    }

    SkFontArguments::VariationPosition::Coordinate width_coordinate = {
        kWdthTag, SkFloatToScalar(selection_capabilities.width.clampToRange(
                      selection_request.width))};
    absl::optional<SkFontParameters::Variation::Axis> wdth_parameters =
        RetrieveVariationDesignParametersByTag(base_typeface_, kWdthTag);
    if (selection_capabilities.width.IsRangeSetFromAuto() && wdth_parameters) {
      DCHECK(RuntimeEnabledFeatures::CSSFontFaceAutoVariableRangeEnabled());
      FontSelectionRange wdth_range = {
          FontSelectionValue(wdth_parameters->min),
          FontSelectionValue(wdth_parameters->max)};
      width_coordinate = {
          kWdthTag,
          SkFloatToScalar(wdth_range.clampToRange(selection_request.width))};
    }
    // CSS and OpenType have opposite definitions of direction of slant
    // angle. In OpenType positive values turn counter-clockwise, negative
    // values clockwise - in CSS positive values are clockwise rotations /
    // skew. See note in https://drafts.csswg.org/css-fonts/#font-style-prop -
    // map value from CSS to OpenType here.
    SkFontArguments::VariationPosition::Coordinate slant_coordinate = {
        kSlntTag, SkFloatToScalar(-selection_capabilities.slope.clampToRange(
                      selection_request.slope))};
    absl::optional<SkFontParameters::Variation::Axis> slnt_parameters =
        RetrieveVariationDesignParametersByTag(base_typeface_, kSlntTag);
    if (selection_capabilities.slope.IsRangeSetFromAuto() && slnt_parameters) {
      DCHECK(RuntimeEnabledFeatures::CSSFontFaceAutoVariableRangeEnabled());
      FontSelectionRange slnt_range = {
          FontSelectionValue(slnt_parameters->min),
          FontSelectionValue(slnt_parameters->max)};
      slant_coordinate = {
          kSlntTag,
          SkFloatToScalar(slnt_range.clampToRange(-selection_request.slope))};
      synthetic_italic = italic && slnt_range.maximum < ItalicSlopeValue() &&
                         selection_request.slope >= ItalicSlopeValue();
    }

    variation.push_back(weight_coordinate);
    variation.push_back(width_coordinate);
    variation.push_back(slant_coordinate);

    bool explicit_opsz_configured = false;
    if (variation_settings && variation_settings->size() < UINT16_MAX) {
      variation.reserve(variation_settings->size() + variation.size());
      for (const auto& setting : *variation_settings) {
        if (setting.Tag() == kOpszTag)
          explicit_opsz_configured = true;
        SkFontArguments::VariationPosition::Coordinate setting_coordinate =
            {setting.Tag(), SkFloatToScalar(setting.Value())};
        variation.push_back(setting_coordinate);
      }
    }

    if (!explicit_opsz_configured) {
      if (optical_sizing == kAutoOpticalSizing) {
        SkFontArguments::VariationPosition::Coordinate opsz_coordinate = {
            kOpszTag, SkFloatToScalar(adjusted_specified_size)};
        variation.push_back(opsz_coordinate);
      } else if (optical_sizing == kNoneOpticalSizing) {
        // Explicitly set default value to avoid automatic application of
        // optical sizing as it seems to happen on SkTypeface on Mac.
        absl::optional<SkFontParameters::Variation::Axis> opsz_parameters =
            RetrieveVariationDesignParametersByTag(return_typeface, kOpszTag);
        if (opsz_parameters) {
          float opszDefault = opsz_parameters->def;
          SkFontArguments::VariationPosition::Coordinate opsz_coordinate = {
              kOpszTag, SkFloatToScalar(opszDefault)};
          variation.push_back(opsz_coordinate);
        }
      }
    }

    SkFontArguments font_args;
    font_args.setVariationDesignPosition(
        {variation.data(), static_cast<int>(variation.size())});
    sk_sp<SkTypeface> sk_variation_font(base_typeface_->makeClone(font_args));

    if (sk_variation_font) {
      return_typeface = sk_variation_font;
    } else {
      SkString family_name;
      base_typeface_->getFamilyName(&family_name);
      // TODO: Surface this as a console message?
      LOG(ERROR) << "Unable for apply variation axis properties for font: "
                 << family_name.c_str();
    }
  }

  if (palette && !palette->IsNormalPalette()) {
    // TODO: Check applicability of font-palette-values according to matching
    // font family name, or should that be done at the CSS family level?

    SkFontArguments font_args;
    SkFontArguments::Palette sk_palette{0, nullptr, 0};

    absl::optional<uint16_t> palette_index = absl::nullopt;

    if (palette->GetPaletteNameKind() == FontPalette::kLightPalette ||
        palette->GetPaletteNameKind() == FontPalette::kDarkPalette) {
      OpenTypeCpalLookup::PaletteUse palette_use =
          palette->GetPaletteNameKind() == FontPalette::kLightPalette
              ? OpenTypeCpalLookup::kUsableWithLightBackground
              : OpenTypeCpalLookup::kUsableWithDarkBackground;
      palette_index =
          OpenTypeCpalLookup::FirstThemedPalette(base_typeface_, palette_use);
    } else if (palette->IsCustomPalette()) {
      FontPalette::BasePaletteValue base_palette_index =
          palette->GetBasePalette();

      switch (base_palette_index.type) {
        case FontPalette::kNoBasePalette: {
          palette_index = 0;
          break;
        }
        case FontPalette::kDarkBasePalette: {
          OpenTypeCpalLookup::PaletteUse palette_use =
              OpenTypeCpalLookup::kUsableWithDarkBackground;
          palette_index = OpenTypeCpalLookup::FirstThemedPalette(base_typeface_,
                                                                 palette_use);
          break;
        }
        case FontPalette::kLightBasePalette: {
          OpenTypeCpalLookup::PaletteUse palette_use =
              OpenTypeCpalLookup::kUsableWithLightBackground;
          palette_index = OpenTypeCpalLookup::FirstThemedPalette(base_typeface_,
                                                                 palette_use);
          break;
        }
        case FontPalette::kIndexBasePalette: {
          palette_index = base_palette_index.index;
          break;
        }
      }
    }

    if (palette_index.has_value()) {
      sk_palette.index = *palette_index;

      auto* color_overrides = palette->GetColorOverrides();
      if (color_overrides && color_overrides->size()) {
        sk_palette.overrides =
            reinterpret_cast<const SkFontArguments::Palette::Override*>(
                color_overrides->data());
        sk_palette.overrideCount = color_overrides->size();
      }

      font_args.setPalette(sk_palette);
    }

    sk_sp<SkTypeface> palette_typeface(return_typeface->makeClone(font_args));
    if (palette_typeface) {
      return_typeface = palette_typeface;
    }
  }

  return FontPlatformData(std::move(return_typeface), std::string(), size,
                          synthetic_bold && !base_typeface_->isBold(),
                          synthetic_italic && !base_typeface_->isItalic(),
                          text_rendering, resolved_font_features, orientation);
}

Vector<VariationAxis> FontCustomPlatformData::GetVariationAxes() const {
  return VariableAxesNames::GetVariationAxes(base_typeface_);
}

String FontCustomPlatformData::FamilyNameForInspector() const {
  SkTypeface::LocalizedStrings* font_family_iterator =
      base_typeface_->createFamilyNameIterator();
  SkTypeface::LocalizedString localized_string;
  while (font_family_iterator->next(&localized_string)) {
    // BCP 47 tags for English take precedent in font matching over other
    // localizations: https://drafts.csswg.org/css-fonts/#descdef-src.
    if (localized_string.fLanguage.equals("en") ||
        localized_string.fLanguage.equals("en-US")) {
      break;
    }
  }
  font_family_iterator->unref();
  return String::FromUTF8(localized_string.fString.c_str(),
                          localized_string.fString.size());
}

scoped_refptr<FontCustomPlatformData> FontCustomPlatformData::Create(
    SharedBuffer* buffer,
    String& ots_parse_message) {
  DCHECK(buffer);
  WebFontDecoder decoder;
  sk_sp<SkTypeface> typeface = decoder.Decode(buffer);
  if (!typeface) {
    ots_parse_message = decoder.GetErrorString();
    return nullptr;
  }
  return base::AdoptRef(
      new FontCustomPlatformData(std::move(typeface), decoder.DecodedSize()));
}

bool FontCustomPlatformData::MayBeIconFont() const {
  if (!may_be_icon_font_computed_) {
    // We observed that many icon fonts define almost all of their glyphs in the
    // Unicode Private Use Area, while non-icon fonts rarely use PUA. We use
    // this as a heuristic to determine if a font is an icon font.

    // We first obtain the list of glyphs mapped from PUA codepoint range:
    // https://unicode.org/charts/PDF/UE000.pdf
    // Note: The two supplementary PUA here are too long but not used much by
    // icon fonts, so we don't include them in this heuristic.
    wtf_size_t pua_length =
        kPrivateUseLastCharacter - kPrivateUseFirstCharacter + 1;
    Vector<SkUnichar> pua_codepoints(pua_length);
    for (wtf_size_t i = 0; i < pua_length; ++i)
      pua_codepoints[i] = kPrivateUseFirstCharacter + i;

    Vector<SkGlyphID> glyphs(pua_codepoints.size());
    base_typeface_->unicharsToGlyphs(pua_codepoints.data(),
                                     pua_codepoints.size(), glyphs.data());

    // Deduplicate and exclude glyph ID 0 (which means undefined glyph)
    std::sort(glyphs.begin(), glyphs.end());
    glyphs.erase(std::unique(glyphs.begin(), glyphs.end()), glyphs.end());
    if (!glyphs[0])
      glyphs.EraseAt(0);

    // We use the heuristic that if more than half of the define glyphs are in
    // PUA, then the font may be an icon font.
    wtf_size_t pua_glyph_count = glyphs.size();
    wtf_size_t total_glyphs = base_typeface_->countGlyphs();
    may_be_icon_font_ = pua_glyph_count * 2 > total_glyphs;
  }
  return may_be_icon_font_;
}

}  // namespace blink
