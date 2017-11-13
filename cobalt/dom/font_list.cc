// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/dom/font_list.h"

#include "base/i18n/char_iterator.h"
#include "cobalt/dom/font_cache.h"

namespace cobalt {
namespace dom {

namespace {

const char16 kHorizontalEllipsisValue = 0x2026;

}  // namespace

FontList::FontList(FontCache* font_cache, const FontListKey& font_list_key)
    : font_cache_(font_cache),
      style_(font_list_key.style),
      size_(font_list_key.size),
      is_font_metrics_set_(false),
      font_metrics_(0, 0, 0, 0),
      is_space_width_set_(false),
      space_width_(0),
      is_ellipsis_info_set_(false),
      ellipsis_width_(0),
      character_fallback_typeface_map_(
          font_cache_->GetCharacterFallbackTypefaceMap(style_)) {
  // Add all of the family names to the font list fonts.
  for (size_t i = 0; i < font_list_key.family_names.size(); ++i) {
    fonts_.push_back(FontListFont(font_list_key.family_names[i]));
  }

  // Add an empty font at the end in order to fall back to the default typeface.
  fonts_.push_back(FontListFont(""));
}

bool FontList::IsVisible() const {
  for (size_t i = 0; i < fonts_.size(); ++i) {
    // While any font in the font list is loading with an active timer, the font
    // is made transparent. "In cases where textual content is loaded before
    // downloadable fonts are available, user agents may... render text
    // transparently with fallback fonts to avoid a flash of  text using a
    // fallback font. In cases where the font download fails user agents must
    // display text, simply leaving transparent text is considered
    // non-conformant behavior."
    //   https://www.w3.org/TR/css3-fonts/#font-face-loading
    if (fonts_[i].state() == FontListFont::kLoadingWithTimerActiveState) {
      return false;
    }
  }

  return true;
}

void FontList::ResetLoadingFonts() {
  bool found_loaded_font = false;

  for (size_t i = 0; i < fonts_.size(); ++i) {
    FontListFont& font_list_font = fonts_[i];

    if (font_list_font.state() == FontListFont::kLoadingWithTimerActiveState ||
        font_list_font.state() == FontListFont::kLoadingWithTimerExpiredState) {
      font_list_font.set_state(FontListFont::kUnrequestedState);
      // If a loaded font hasn't been found yet, then the cached values need to
      // be reset. It'll potentially change the primary font.
      if (!found_loaded_font) {
        primary_font_ = NULL;
        is_font_metrics_set_ = false;
        is_space_width_set_ = false;
        is_ellipsis_info_set_ = false;
        ellipsis_font_ = NULL;
      }
    } else if (font_list_font.state() == FontListFont::kLoadedState) {
      found_loaded_font = true;
    }
  }
}

scoped_refptr<render_tree::GlyphBuffer> FontList::CreateGlyphBuffer(
    const char16* text_buffer, int32 text_length, bool is_rtl) {
  return font_cache_->CreateGlyphBuffer(text_buffer, text_length, is_rtl, this);
}

float FontList::GetTextWidth(const char16* text_buffer, int32 text_length,
                             bool is_rtl,
                             render_tree::FontVector* maybe_used_fonts) {
  return font_cache_->GetTextWidth(text_buffer, text_length, is_rtl, this,
                                   maybe_used_fonts);
}

const render_tree::FontMetrics& FontList::GetFontMetrics() {
  // The font metrics are lazily generated. If they haven't been set yet, it's
  // time to set them.
  if (!is_font_metrics_set_) {
    is_font_metrics_set_ = true;
    font_metrics_ = GetPrimaryFont()->GetFontMetrics();
  }

  return font_metrics_;
}

render_tree::FontMetrics FontList::GetFontMetrics(
    const render_tree::FontVector& fonts) {
  // Call GetFontMetrics to ensure that the primary metrics have been
  // generated.
  render_tree::TypefaceId primary_typeface_id =
      GetPrimaryFont()->GetTypefaceId();
  const render_tree::FontMetrics& primary_metrics = GetFontMetrics();

  // Initially set the font metrics values to the primary font metrics. It is
  // included regardless of the the contents of the vector.
  float max_ascent = primary_metrics.ascent();
  float max_descent = primary_metrics.descent();
  float max_leading = primary_metrics.leading();
  float max_x_height = primary_metrics.x_height();

  // Calculate the max metrics values from all of the fonts.
  for (size_t i = 0; i < fonts.size(); ++i) {
    // If this is the primary font, simply skip it. It has already been
    // included.
    if (fonts[i]->GetTypefaceId() == primary_typeface_id) {
      continue;
    }

    render_tree::FontMetrics current_metrics = fonts[i]->GetFontMetrics();
    if (current_metrics.ascent() > max_ascent) {
      max_ascent = current_metrics.ascent();
    }
    if (current_metrics.descent() > max_descent) {
      max_descent = current_metrics.descent();
    }
    if (current_metrics.leading() > max_leading) {
      max_leading = current_metrics.leading();
    }
    if (current_metrics.x_height() > max_x_height) {
      max_x_height = current_metrics.x_height();
    }
  }

  return render_tree::FontMetrics(max_ascent, max_descent, max_leading,
                                  max_x_height);
}

char16 FontList::GetEllipsisValue() const { return kHorizontalEllipsisValue; }

const scoped_refptr<render_tree::Font>& FontList::GetEllipsisFont() {
  GenerateEllipsisInfo();
  return ellipsis_font_;
}

float FontList::GetEllipsisWidth() {
  GenerateEllipsisInfo();
  return ellipsis_width_;
}

float FontList::GetSpaceWidth() {
  GenerateSpaceWidth();
  return space_width_;
}

const scoped_refptr<render_tree::Font>& FontList::GetCharacterFont(
    int32 utf32_character, render_tree::GlyphIndex* glyph_index) {
  // Walk the list of fonts, requesting any encountered that are in an
  // unrequested state. The first font encountered that has the character is the
  // character font.
  for (size_t i = 0; i < fonts_.size(); ++i) {
    FontListFont& font_list_font = fonts_[i];

    if (font_list_font.state() == FontListFont::kUnrequestedState) {
      RequestFont(i);
    }

    if (font_list_font.state() == FontListFont::kLoadedState) {
      *glyph_index =
          font_list_font.font()->GetGlyphForCharacter(utf32_character);
      if (*glyph_index != render_tree::kInvalidGlyphIndex) {
        return font_list_font.font();
      }
    }
  }

  return GetFallbackCharacterFont(utf32_character, glyph_index);
}

const scoped_refptr<render_tree::Font>& FontList::GetFallbackCharacterFont(
    int32 utf32_character, render_tree::GlyphIndex* glyph_index) {
  scoped_refptr<render_tree::Typeface>& fallback_typeface =
      character_fallback_typeface_map_[utf32_character];
  if (fallback_typeface == NULL) {
    fallback_typeface =
        font_cache_->GetCharacterFallbackTypeface(utf32_character, style_);
  }

  *glyph_index = fallback_typeface->GetGlyphForCharacter(utf32_character);

  // Check to see if the typeface id already maps to a specific font. If it does
  // simply return that font.
  scoped_refptr<render_tree::Font>& fallback_font =
      fallback_typeface_to_font_map_[fallback_typeface->GetId()];
  if (fallback_font == NULL) {
    fallback_font = fallback_typeface_to_font_map_[fallback_typeface->GetId()] =
        font_cache_->GetFontFromTypefaceAndSize(fallback_typeface, size_);
  }

  return fallback_font;
}

const scoped_refptr<render_tree::Font>& FontList::GetPrimaryFont() {
  // The primary font is lazily generated. If it hasn't been set yet, then it's
  // time to do it now.
  if (!primary_font_) {
    // Walk the list of fonts, requesting any encountered that are in an
    // unrequested state. The first font encountered that is loaded is
    // the primary font.
    for (size_t i = 0; i < fonts_.size(); ++i) {
      FontListFont& font_list_font = fonts_[i];

      if (font_list_font.state() == FontListFont::kUnrequestedState) {
        RequestFont(i);
      }

      if (font_list_font.state() == FontListFont::kLoadedState) {
        primary_font_ = font_list_font.font();
        break;
      }
    }
  }
  DCHECK(primary_font_);

  return primary_font_;
}

void FontList::RequestFont(size_t index) {
  FontListFont& font_list_font = fonts_[index];
  FontListFont::State state;

  // Request the font from the font cache; the state of the font will be set
  // during the call.
  scoped_refptr<render_tree::Font> render_tree_font = font_cache_->TryGetFont(
      font_list_font.family_name(), style_, size_, &state);

  if (state == FontListFont::kLoadedState) {
    DCHECK(render_tree_font != NULL);

    // Walk all of the fonts in the list preceding the loaded font. If they have
    // the same typeface as the loaded font, then set the font list font as a
    // duplicate. There's no reason to have multiple fonts in the list with the
    // same typeface.
    render_tree::TypefaceId typeface_id = render_tree_font->GetTypefaceId();
    for (size_t i = 0; i < index; ++i) {
      FontListFont& check_font = fonts_[i];
      if (check_font.state() == FontListFont::kLoadedState &&
          check_font.font()->GetTypefaceId() == typeface_id) {
        font_list_font.set_state(FontListFont::kDuplicateState);
        break;
      }
    }

    // If this font wasn't a duplicate, then its time to initialize its font
    // data. This font is now available to use.
    if (font_list_font.state() != FontListFont::kDuplicateState) {
      font_list_font.set_state(FontListFont::kLoadedState);
      font_list_font.set_font(render_tree_font);
    }
  } else {
    font_list_font.set_state(state);
  }
}

void FontList::GenerateEllipsisInfo() {
  if (!is_ellipsis_info_set_) {
    render_tree::GlyphIndex ellipsis_glyph = render_tree::kInvalidGlyphIndex;
    ellipsis_font_ = GetCharacterFont(GetEllipsisValue(), &ellipsis_glyph);
    ellipsis_width_ = ellipsis_font_->GetGlyphWidth(ellipsis_glyph);

    is_ellipsis_info_set_ = true;
  }
}

void FontList::GenerateSpaceWidth() {
  if (!is_space_width_set_) {
    const scoped_refptr<render_tree::Font>& primary_font = GetPrimaryFont();
    render_tree::GlyphIndex space_glyph =
        primary_font->GetGlyphForCharacter(' ');
    space_width_ = primary_font->GetGlyphWidth(space_glyph);
    if (space_width_ == 0) {
      DLOG(WARNING) << "Font being used with space width of 0!";
    }

    is_space_width_set_ = true;
  }
}

}  // namespace dom
}  // namespace cobalt
