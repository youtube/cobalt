/*
 * Copyright (C) 2007 Nicholas Shanks <contact@nickshanks.com>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "build/build_config.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/public/platform/web_font_description.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#endif

namespace blink {

struct SameSizeAsFontDescription {
  DISALLOW_NEW();
  FontFamily family_list;
  scoped_refptr<FontFeatureSettings> feature_settings_;
  scoped_refptr<FontVariationSettings> variation_settings_;
  scoped_refptr<FontPalette> palette_;
  scoped_refptr<FontVariantAlternates> font_variant_alternates_;
  AtomicString locale;
  float sizes[5];
  FontSizeAdjust size_adjust_;
  FontSelectionRequest selection_request_;
  FieldsAsUnsignedType bitfields;
};

ASSERT_SIZE(FontDescription, SameSizeAsFontDescription);

TypesettingFeatures FontDescription::default_typesetting_features_ = 0;

bool FontDescription::use_subpixel_text_positioning_ = false;

// static
FontDescription FontDescription::CreateHashTableEmptyValue() {
  FontDescription result;
  memset(&result, 0, sizeof(FontDescription));
  DCHECK(result.IsHashTableEmptyValue());
  return result;
}

FontDescription::FontDescription(WTF::HashTableDeletedValueType) {
  memset(this, 0, sizeof(FontDescription));
  fields_.hash_category_ = kHashDeletedValue;
}

FontDescription::FontDescription()
    : specified_size_(0),
      computed_size_(0),
      adjusted_size_(0),
      letter_spacing_(0),
      word_spacing_(0),
      font_selection_request_(NormalWeightValue(),
                              NormalWidthValue(),
                              NormalSlopeValue()) {
  fields_as_unsigned_.parts[0] = 0;
  fields_as_unsigned_.parts[1] = 0;
  fields_.orientation_ = static_cast<unsigned>(FontOrientation::kHorizontal);
  fields_.width_variant_ = kRegularWidth;
  fields_.variant_caps_ = kCapsNormal;
  fields_.is_absolute_size_ = false;
  fields_.generic_family_ = kNoFamily;
  fields_.kerning_ = kAutoKerning;
  fields_.common_ligatures_state_ = kNormalLigaturesState;
  fields_.discretionary_ligatures_state_ = kNormalLigaturesState;
  fields_.historical_ligatures_state_ = kNormalLigaturesState;
  fields_.contextual_ligatures_state_ = kNormalLigaturesState;
  fields_.keyword_size_ = 0;
  fields_.font_smoothing_ = kAutoSmoothing;
  fields_.text_rendering_ = kAutoTextRendering;
  fields_.synthetic_bold_ = false;
  fields_.synthetic_italic_ = false;
  fields_.subpixel_text_position_ = use_subpixel_text_positioning_;
  fields_.typesetting_features_ = default_typesetting_features_;
  fields_.variant_numeric_ = FontVariantNumeric().fields_as_unsigned_;
  fields_.subpixel_ascent_descent_ = false;
  fields_.font_optical_sizing_ = OpticalSizing::kAutoOpticalSizing;
  fields_.hash_category_ = kHashRegularValue;
  fields_.font_synthesis_weight_ = kAutoFontSynthesisWeight;
  fields_.font_synthesis_style_ = kAutoFontSynthesisStyle;
  fields_.font_synthesis_small_caps_ = kAutoFontSynthesisSmallCaps;
  fields_.variant_position_ = kNormalVariantPosition;
}

FontDescription::FontDescription(const FontDescription&) = default;

FontDescription& FontDescription::operator=(const FontDescription&) = default;

bool FontDescription::operator==(const FontDescription& other) const {
  return family_list_ == other.family_list_ && locale_ == other.locale_ &&
         specified_size_ == other.specified_size_ &&
         computed_size_ == other.computed_size_ &&
         adjusted_size_ == other.adjusted_size_ &&
         size_adjust_ == other.size_adjust_ &&
         letter_spacing_ == other.letter_spacing_ &&
         word_spacing_ == other.word_spacing_ &&
         font_selection_request_ == other.font_selection_request_ &&
         fields_as_unsigned_.parts[0] == other.fields_as_unsigned_.parts[0] &&
         fields_as_unsigned_.parts[1] == other.fields_as_unsigned_.parts[1] &&
         (feature_settings_ == other.feature_settings_ ||
          (feature_settings_ && other.feature_settings_ &&
           *feature_settings_ == *other.feature_settings_)) &&
         (variation_settings_ == other.variation_settings_ ||
          (variation_settings_ && other.variation_settings_ &&
           *variation_settings_ == *other.variation_settings_)) &&
         base::ValuesEquivalent(font_palette_, other.font_palette_) &&
         base::ValuesEquivalent(font_variant_alternates_,
                                other.font_variant_alternates_);
}

// Compute a 'lighter' weight per
// https://drafts.csswg.org/css-fonts-4/#font-weight-prop
FontSelectionValue FontDescription::LighterWeight(FontSelectionValue weight) {
  DCHECK(weight >= FontSelectionValue(1) && weight <= FontSelectionValue(1000));
  // [1, 100) => No change
  if (weight < FontSelectionValue(100))
    return weight;
  // [100, 550) => 100
  if (weight < FontSelectionValue(550))
    return FontSelectionValue(100);
  // [550, 750) => 400
  if (weight < FontSelectionValue(750))
    return FontSelectionValue(400);
  // [750, 1000] => 700
  return FontSelectionValue(700);
}

// Compute a 'bolder' weight per
// https://drafts.csswg.org/css-fonts-4/#font-weight-prop
FontSelectionValue FontDescription::BolderWeight(FontSelectionValue weight) {
  DCHECK(weight >= FontSelectionValue(1) && weight <= FontSelectionValue(1000));
  // [1, 350) => 400
  if (weight < FontSelectionValue(350))
    return FontSelectionValue(400);
  // [350, 550) => 700
  if (weight < FontSelectionValue(550))
    return FontSelectionValue(700);
  // [550, 900) => 900
  if (weight < FontSelectionValue(900))
    return FontSelectionValue(900);
  // [900, 1000] => No change
  return weight;
}

FontDescription::Size FontDescription::LargerSize(const Size& size) {
  return Size(0, size.value * 1.2, size.is_absolute);
}

FontDescription::Size FontDescription::SmallerSize(const Size& size) {
  return Size(0, size.value / 1.2, size.is_absolute);
}

FontSelectionRequest FontDescription::GetFontSelectionRequest() const {
  return font_selection_request_;
}

FontDescription::VariantLigatures FontDescription::GetVariantLigatures() const {
  VariantLigatures ligatures;

  ligatures.common = CommonLigaturesState();
  ligatures.discretionary = DiscretionaryLigaturesState();
  ligatures.historical = HistoricalLigaturesState();
  ligatures.contextual = ContextualLigaturesState();

  return ligatures;
}

void FontDescription::SetVariantCaps(FontVariantCaps variant_caps) {
  fields_.variant_caps_ = variant_caps;

  UpdateTypesettingFeatures();
}

void FontDescription::SetVariantEastAsian(
    const FontVariantEastAsian variant_east_asian) {
  fields_.variant_east_asian_ = variant_east_asian.fields_as_unsigned_;
}

void FontDescription::SetVariantLigatures(const VariantLigatures& ligatures) {
  fields_.common_ligatures_state_ = ligatures.common;
  fields_.discretionary_ligatures_state_ = ligatures.discretionary;
  fields_.historical_ligatures_state_ = ligatures.historical;
  fields_.contextual_ligatures_state_ = ligatures.contextual;

  UpdateTypesettingFeatures();
}

void FontDescription::SetVariantNumeric(
    const FontVariantNumeric& variant_numeric) {
  fields_.variant_numeric_ = variant_numeric.fields_as_unsigned_;

  UpdateTypesettingFeatures();
}

float FontDescription::EffectiveFontSize() const {
  // Ensure that the effective precision matches the font-cache precision.
  // This guarantees that the same precision is used regardless of cache status.
  // Note: HasSizeAdjust() is for the font-size-adjust property, not for the
  // size-adjust descriptor.
  float computed_or_adjusted_size =
      HasSizeAdjust() || fields_.has_size_adjust_descriptor_ ? AdjustedSize()
                                                             : ComputedSize();
  return floorf(computed_or_adjusted_size *
                FontCacheKey::PrecisionMultiplier()) /
         FontCacheKey::PrecisionMultiplier();
}

float FontDescription::AdjustedSpecifiedSize() const {
  if (HasSizeAdjust() || fields_.has_size_adjust_descriptor_) {
    return SpecifiedSize() * (AdjustedSize() / ComputedSize());
  } else {
    return SpecifiedSize();
  }
}

FontDescription FontDescription::SizeAdjustedFontDescription(
    float size_adjust) const {
  // TODO(crbug.com/451346): The font-size-adjust property and size-adjust
  // descriptor currently don't work together. For sanity, if both are set, we
  // ignore size-adjust. Fix it when shipping font-size-adjust.
  if (HasSizeAdjust())
    return *this;

  // size-adjust should be applied at most once.
  DCHECK(!fields_.has_size_adjust_descriptor_);
  FontDescription result(*this);
  result.SetAdjustedSize(ComputedSize() * size_adjust);
  result.fields_.has_size_adjust_descriptor_ = true;
  return result;
}

FontCacheKey FontDescription::CacheKey(
    const FontFaceCreationParams& creation_params,
    bool is_unique_match,
    bool is_generic_family) const {
  unsigned options =
      static_cast<unsigned>(fields_.font_optical_sizing_) << 7 |  // bit 8
      static_cast<unsigned>(fields_.synthetic_italic_) << 6 |     // bit 7
      static_cast<unsigned>(fields_.synthetic_bold_) << 5 |       // bit 6
      static_cast<unsigned>(fields_.text_rendering_) << 3 |       // bits 4-5
      static_cast<unsigned>(fields_.orientation_) << 1 |          // bit 2-3
      static_cast<unsigned>(fields_.subpixel_text_position_);     // bit 1

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  float device_scale_factor_for_key = FontCache::DeviceScaleFactor();
#else
  float device_scale_factor_for_key = 1.0f;
#endif
  FontCacheKey cache_key(creation_params, EffectiveFontSize(),
                         options | font_selection_request_.GetHash() << 9,
                         device_scale_factor_for_key, variation_settings_,
                         font_palette_, font_variant_alternates_,
                         is_unique_match, is_generic_family);
#if BUILDFLAG(IS_ANDROID)
  if (const LayoutLocale* locale = Locale()) {
    if (FontCache::GetLocaleSpecificFamilyName(creation_params.Family()))
      cache_key.SetLocale(locale->LocaleForSkFontMgr());
  }
#endif  // BUILDFLAG(IS_ANDROID)
  return cache_key;
}

void FontDescription::SetDefaultTypesettingFeatures(
    TypesettingFeatures typesetting_features) {
  default_typesetting_features_ = typesetting_features;
}

TypesettingFeatures FontDescription::DefaultTypesettingFeatures() {
  return default_typesetting_features_;
}

void FontDescription::UpdateTypesettingFeatures() {
  fields_.typesetting_features_ = default_typesetting_features_;

  switch (TextRendering()) {
    case kAutoTextRendering:
      break;
    case kOptimizeSpeed:
      fields_.typesetting_features_ &= ~(blink::kKerning | kLigatures);
      break;
    case kGeometricPrecision:
    case kOptimizeLegibility:
      fields_.typesetting_features_ |= blink::kKerning | kLigatures;
      break;
  }

  switch (GetKerning()) {
    case FontDescription::kNoneKerning:
      fields_.typesetting_features_ &= ~blink::kKerning;
      break;
    case FontDescription::kNormalKerning:
      fields_.typesetting_features_ |= blink::kKerning;
      break;
    case FontDescription::kAutoKerning:
      break;
  }

  // As per CSS (https://drafts.csswg.org/css-text/#letter-spacing-property),
  // When the effective letter-spacing between two characters is not zero (due
  // to either justification or non-zero computed letter-spacing), user agents
  // should not apply optional ligatures.
  if (letter_spacing_ == 0) {
    switch (CommonLigaturesState()) {
      case FontDescription::kDisabledLigaturesState:
        fields_.typesetting_features_ &= ~blink::kLigatures;
        break;
      case FontDescription::kEnabledLigaturesState:
        fields_.typesetting_features_ |= blink::kLigatures;
        break;
      case FontDescription::kNormalLigaturesState:
        break;
    }

    if (DiscretionaryLigaturesState() ==
            FontDescription::kEnabledLigaturesState ||
        HistoricalLigaturesState() == FontDescription::kEnabledLigaturesState ||
        ContextualLigaturesState() == FontDescription::kEnabledLigaturesState) {
      fields_.typesetting_features_ |= blink::kLigatures;
    }
  }

  if (VariantCaps() != kCapsNormal)
    fields_.typesetting_features_ |= blink::kCaps;
}

namespace {

// This converts -0.0 to 0.0, so that they have the same hash value. This
// ensures that equal FontDescription have the same hash value.
float NormalizeSign(float number) {
  if (UNLIKELY(number == 0.0))
    return 0.0;
  return number;
}

}  // namespace

unsigned FontDescription::StyleHashWithoutFamilyList() const {
  unsigned hash = 0;
  StringHasher string_hasher;
  const FontFeatureSettings* settings = FeatureSettings();
  if (settings) {
    unsigned num_features = settings->size();
    for (unsigned i = 0; i < num_features; ++i) {
      WTF::AddIntToHash(hash, settings->at(i).Tag());
      WTF::AddIntToHash(hash, settings->at(i).Value());
    }
  }

  if (VariationSettings())
    WTF::AddIntToHash(hash, VariationSettings()->GetHash());

  if (locale_) {
    const AtomicString& locale = locale_->LocaleString();
    for (unsigned i = 0; i < locale.length(); i++)
      string_hasher.AddCharacter(locale[i]);
  }
  WTF::AddIntToHash(hash, string_hasher.GetHash());

  WTF::AddFloatToHash(hash, NormalizeSign(specified_size_));
  WTF::AddFloatToHash(hash, NormalizeSign(computed_size_));
  WTF::AddFloatToHash(hash, NormalizeSign(adjusted_size_));
  WTF::AddFloatToHash(hash, NormalizeSign(size_adjust_.Value()));
  WTF::AddFloatToHash(hash, NormalizeSign(letter_spacing_));
  WTF::AddFloatToHash(hash, NormalizeSign(word_spacing_));
  WTF::AddIntToHash(hash, fields_as_unsigned_.parts[0]);
  WTF::AddIntToHash(hash, fields_as_unsigned_.parts[1]);
  WTF::AddIntToHash(hash, font_selection_request_.GetHash());

  return hash;
}

unsigned FontDescription::GetHash() const {
  unsigned hash = StyleHashWithoutFamilyList();
  for (const FontFamily* family = &family_list_; family;
       family = family->Next()) {
    if (family->FamilyName().empty())
      continue;
    WTF::AddIntToHash(hash, family->FamilyIsGeneric());
    WTF::AddIntToHash(hash, WTF::GetHash(family->FamilyName()));
  }
  return hash;
}

void FontDescription::SetOrientation(FontOrientation orientation) {
  fields_.orientation_ = static_cast<unsigned>(orientation);
  UpdateSyntheticOblique();
}

void FontDescription::SetStyle(FontSelectionValue value) {
  font_selection_request_.slope = value;
  original_slope = value;
  UpdateSyntheticOblique();
}

void FontDescription::UpdateSyntheticOblique() {
  // Doing synthetic oblique for vertical writing mode with upright text
  // orientation when negative angle parameter of "oblique" keyword, e.g.
  // "font-style: oblique -15deg" for simulating "tts:fontShear"[1][2], we
  // need to have normal font style instead of italic/oblique.
  // [1]
  // https://www.w3.org/TR/2018/REC-ttml2-20181108/#style-attribute-fontShear
  // [2] See http://crbug.com/1112923
  fields_.synthetic_oblique_ =
      IsVerticalAnyUpright() && original_slope < FontSelectionValue(0);
  font_selection_request_.slope =
      fields_.synthetic_oblique_ ? NormalSlopeValue() : original_slope;
}

SkFontStyle FontDescription::SkiaFontStyle() const {
  // FIXME(drott): This is a lossy conversion, compare
  // https://bugs.chromium.org/p/skia/issues/detail?id=6844
  int skia_width = SkFontStyle::kNormal_Width;
  if (Stretch() <= UltraCondensedWidthValue())
    skia_width = SkFontStyle::kUltraCondensed_Width;
  if (Stretch() <= ExtraCondensedWidthValue())
    skia_width = SkFontStyle::kExtraCondensed_Width;
  if (Stretch() <= CondensedWidthValue())
    skia_width = SkFontStyle::kCondensed_Width;
  if (Stretch() <= SemiCondensedWidthValue())
    skia_width = SkFontStyle::kSemiCondensed_Width;
  if (Stretch() >= SemiExpandedWidthValue())
    skia_width = SkFontStyle::kSemiExpanded_Width;
  if (Stretch() >= ExpandedWidthValue())
    skia_width = SkFontStyle::kExpanded_Width;
  if (Stretch() >= ExtraExpandedWidthValue())
    skia_width = SkFontStyle::kExtraExpanded_Width;
  if (Stretch() >= UltraExpandedWidthValue())
    skia_width = SkFontStyle::kUltraExpanded_Width;

  SkFontStyle::Slant slant = SkFontStyle::kUpright_Slant;
  FontSelectionValue style = Style();
  if (style > NormalSlopeValue() && style <= ItalicThreshold())
    slant = SkFontStyle::kItalic_Slant;
  if (style > ItalicThreshold()) {
    slant = SkFontStyle::kOblique_Slant;
  }

  int skia_weight = SkFontStyle::kNormal_Weight;
  if (Weight() >= MinWeightValue() && Weight() <= MaxWeightValue())
    skia_weight = static_cast<int>(Weight());

  return SkFontStyle(skia_weight, skia_width, slant);
}

void FontDescription::UpdateFromSkiaFontStyle(const SkFontStyle& font_style) {
  SetWeight(FontSelectionValue(font_style.weight()));

  switch (font_style.width()) {
    case (SkFontStyle::kUltraCondensed_Width):
      SetStretch(UltraCondensedWidthValue());
      break;
    case (SkFontStyle::kExtraCondensed_Width):
      SetStretch(ExtraCondensedWidthValue());
      break;
    case (SkFontStyle::kCondensed_Width):
      SetStretch(CondensedWidthValue());
      break;
    case (SkFontStyle::kSemiCondensed_Width):
      SetStretch(SemiCondensedWidthValue());
      break;
    case (SkFontStyle::kSemiExpanded_Width):
      SetStretch(SemiExpandedWidthValue());
      break;
    case (SkFontStyle::kExpanded_Width):
      SetStretch(ExpandedWidthValue());
      break;
    case (SkFontStyle::kExtraExpanded_Width):
      SetStretch(ExtraExpandedWidthValue());
      break;
    case (SkFontStyle::kUltraExpanded_Width):
      SetStretch(UltraExpandedWidthValue());
      break;
  }

  if (font_style.slant() == SkFontStyle::kOblique_Slant)
    SetStyle(ItalicSlopeValue());
  else
    SetStyle(NormalSlopeValue());
}

int FontDescription::MinimumPrefixWidthToHyphenate() const {
  // If the maximum width available for the prefix before the hyphen is small,
  // then it is very unlikely that an hyphenation opportunity exists, so do not
  // bother to look for it.  These are heuristic numbers for performance added
  // in http://wkb.ug/45606
  const int kMinimumPrefixWidthNumerator = 5;
  const int kMinimumPrefixWidthDenominator = 4;
  return ComputedPixelSize() * kMinimumPrefixWidthNumerator /
         kMinimumPrefixWidthDenominator;
}

String FontDescription::ToString(GenericFamilyType familyType) {
  switch (familyType) {
    case GenericFamilyType::kNoFamily:
      return "None";
    case GenericFamilyType::kStandardFamily:
      return "Standard";
    case GenericFamilyType::kWebkitBodyFamily:
      return "WebkitBody";
    case GenericFamilyType::kSerifFamily:
      return "Serif";
    case GenericFamilyType::kSansSerifFamily:
      return "SansSerif";
    case GenericFamilyType::kMonospaceFamily:
      return "Monospace";
    case GenericFamilyType::kCursiveFamily:
      return "Cursive";
    case GenericFamilyType::kFantasyFamily:
      return "Fantasy";
  }
  return "Unknown";
}

String FontDescription::ToString(LigaturesState state) {
  switch (state) {
    case LigaturesState::kNormalLigaturesState:
      return "Normal";
    case LigaturesState::kDisabledLigaturesState:
      return "Disabled";
    case LigaturesState::kEnabledLigaturesState:
      return "Enabled";
  }
  return "Unknown";
}

String FontDescription::ToString(Kerning kerning) {
  switch (kerning) {
    case Kerning::kAutoKerning:
      return "Auto";
    case Kerning::kNormalKerning:
      return "Normal";
    case Kerning::kNoneKerning:
      return "None";
  }
  return "Unknown";
}

String FontDescription::ToString(FontSelectionValue selection_value) {
  if (selection_value == UltraCondensedWidthValue())
    return "Ultra-Condensed";
  else if (selection_value == ExtraCondensedWidthValue())
    return "Extra-Condensed";
  else if (selection_value == CondensedWidthValue())
    return "Condensed";
  else if (selection_value == SemiCondensedWidthValue())
    return "Semi-Condensed";
  else if (selection_value == NormalWidthValue())
    return "Normal";
  else if (selection_value == SemiExpandedWidthValue())
    return "Semi-Expanded";
  else if (selection_value == ExpandedWidthValue())
    return "Expanded";
  else if (selection_value == ExtraExpandedWidthValue())
    return "Extra-Expanded";
  else if (selection_value == UltraExpandedWidthValue())
    return "Ultra-Expanded";

  return "Unknown";
}

String FontDescription::ToString(FontVariantCaps variant) {
  switch (variant) {
    case FontVariantCaps::kCapsNormal:
      return "Normal";
    case FontVariantCaps::kSmallCaps:
      return "SmallCaps";
    case FontVariantCaps::kAllSmallCaps:
      return "AllSmallCaps";
    case FontVariantCaps::kPetiteCaps:
      return "PetiteCaps";
    case FontVariantCaps::kAllPetiteCaps:
      return "AllPetiteCaps";
    case FontVariantCaps::kUnicase:
      return "Unicase";
    case FontVariantCaps::kTitlingCaps:
      return "TitlingCaps";
  }
  return "Unknown";
}

String FontDescription::ToStringForIdl(FontVariantCaps variant) {
  switch (variant) {
    case FontVariantCaps::kCapsNormal:
      return "normal";
    case FontVariantCaps::kSmallCaps:
      return "small-caps";
    case FontVariantCaps::kAllSmallCaps:
      return "all-small-caps";
    case FontVariantCaps::kPetiteCaps:
      return "petite-caps";
    case FontVariantCaps::kAllPetiteCaps:
      return "all-petite-caps";
    case FontVariantCaps::kUnicase:
      return "unicase";
    case FontVariantCaps::kTitlingCaps:
      return "titling-caps";
  }
  return "Unknown";
}

String FontDescription::ToString(FontSynthesisWeight font_synthesis_weight) {
  switch (font_synthesis_weight) {
    case FontSynthesisWeight::kAutoFontSynthesisWeight:
      return "Auto";
    case FontSynthesisWeight::kNoneFontSynthesisWeight:
      return "None";
  }
  return "Unknown";
}

String FontDescription::ToString(FontSynthesisStyle font_synthesis_style) {
  switch (font_synthesis_style) {
    case FontSynthesisStyle::kAutoFontSynthesisStyle:
      return "Auto";
    case FontSynthesisStyle::kNoneFontSynthesisStyle:
      return "None";
  }
  return "Unknown";
}

String FontDescription::ToString(
    FontSynthesisSmallCaps font_synthesis_small_caps) {
  switch (font_synthesis_small_caps) {
    case FontSynthesisSmallCaps::kAutoFontSynthesisSmallCaps:
      return "Auto";
    case FontSynthesisSmallCaps::kNoneFontSynthesisSmallCaps:
      return "None";
  }
  return "Unknown";
}

String FontDescription::VariantLigatures::ToString() const {
  return String::Format(
      "common=%s, discretionary=%s, historical=%s, contextual=%s",
      FontDescription::ToString(static_cast<LigaturesState>(common))
          .Ascii()
          .data(),
      FontDescription::ToString(static_cast<LigaturesState>(discretionary))
          .Ascii()
          .data(),
      FontDescription::ToString(static_cast<LigaturesState>(historical))
          .Ascii()
          .data(),
      FontDescription::ToString(static_cast<LigaturesState>(contextual))
          .Ascii()
          .data());
}

String FontDescription::Size::ToString() const {
  return String::Format(
      "keyword_size=%u, specified_size=%f, is_absolute_size=%s", keyword, value,
      is_absolute ? "true" : "false");
}

String FontDescription::FamilyDescription::ToString() const {
  return String::Format(
      "generic_family=%s, family=[%s]",
      FontDescription::ToString(generic_family).Ascii().c_str(),
      family.ToString().Ascii().c_str());
}

String FontDescription::ToString(FontVariantPosition variant_position) {
  switch (variant_position) {
    case FontVariantPosition::kNormalVariantPosition:
      return "Normal";
    case FontVariantPosition::kSubVariantPosition:
      return "Sub";
    case FontVariantPosition::kSuperVariantPosition:
      return "Super";
  }
  return "Unknown";
}

static const char* ToBooleanString(bool value) {
  return value ? "true" : "false";
}

String FontDescription::ToString() const {
  return String::Format(
      "family_list=[%s], feature_settings=[%s], variation_settings=[%s], "
      "locale=%s, "
      "specified_size=%f, computed_size=%f, adjusted_size=%f, "
      "size_adjust=%f, letter_spacing=%f, word_spacing=%f, "
      "font_selection_request=[%s], "
      "typesetting_features=[%s], "
      "orientation=%s, width_variant=%s, variant_caps=%s, "
      "is_absolute_size=%s, generic_family=%s, kerning=%s, "
      "variant_ligatures=[%s], "
      "keyword_size=%u, font_smoothing=%s, text_rendering=%s, "
      "synthetic_bold=%s, synthetic_italic=%s, subpixel_positioning=%s, "
      "subpixel_ascent_descent=%s, variant_numeric=[%s], "
      "variant_east_asian=[%s], font_optical_sizing=%s, "
      "font_synthesis_weight=%s, font_synthesis_style=%s, "
      "font_synthesis_small_caps=%s, font_variant_position=%s",
      family_list_.ToString().Ascii().c_str(),
      (feature_settings_ ? feature_settings_->ToString().Ascii().c_str() : ""),
      (variation_settings_ ? variation_settings_->ToString().Ascii().c_str()
                           : ""),
      // TODO(wkorman): Locale has additional internal fields such as
      // hyphenation and script. Consider adding a more detailed
      // string method.
      (locale_ ? locale_->LocaleString().Ascii().c_str() : ""), specified_size_,
      computed_size_, adjusted_size_, size_adjust_.Value(), letter_spacing_,
      word_spacing_, font_selection_request_.ToString().Ascii().c_str(),
      blink::ToString(
          static_cast<TypesettingFeatures>(fields_.typesetting_features_))
          .Ascii()
          .data(),
      blink::ToString(Orientation()).Ascii().c_str(),
      blink::ToString(WidthVariant()).Ascii().c_str(),
      FontDescription::ToString(VariantCaps()).Ascii().c_str(),
      ToBooleanString(IsAbsoluteSize()),
      FontDescription::ToString(GenericFamily()).Ascii().c_str(),
      FontDescription::ToString(Kerning()).Ascii().c_str(),
      GetVariantLigatures().ToString().Ascii().c_str(), KeywordSize(),
      blink::ToString(FontSmoothing()).Ascii().c_str(),
      blink::ToString(TextRendering()).Ascii().c_str(),
      ToBooleanString(IsSyntheticBold()), ToBooleanString(IsSyntheticItalic()),
      ToBooleanString(UseSubpixelPositioning()),
      ToBooleanString(SubpixelAscentDescent()),
      VariantNumeric().ToString().Ascii().c_str(),
      VariantEastAsian().ToString().Ascii().c_str(),
      blink::ToString(FontOpticalSizing()).Ascii().c_str(),
      FontDescription::ToString(GetFontSynthesisWeight()).Ascii().c_str(),
      FontDescription::ToString(GetFontSynthesisStyle()).Ascii().c_str(),
      FontDescription::ToString(GetFontSynthesisSmallCaps()).Ascii().c_str(),
      FontDescription::ToString(VariantPosition()).Ascii().c_str());
}

}  // namespace blink
