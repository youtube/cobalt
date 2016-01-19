/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/font_list.h"

#include "base/i18n/char_iterator.h"
#include "cobalt/dom/font_cache.h"

namespace cobalt {
namespace dom {

namespace {

const char* kHorizontalEllipsisUtf8Value = "\xe2\x80\xa6";

}  // namespace

FontListFont::FontListFont(const std::string& family_name)
    : family_name_(family_name),
      state_(kUnrequestedState),
      character_glyph_map_(NULL) {}

bool FontListFont::HasGlyphForCharacter(int32 utf32_character) {
  CharacterGlyphMap::iterator map_iterator =
      character_glyph_map_->find(utf32_character);
  if (map_iterator != character_glyph_map_->end()) {
    return map_iterator->second != render_tree::kInvalidGlyphIndex;
  }

  // The character map wasn't already populated with this character's glyph.
  // Both set the character map's glyph and return whether or not it is a valid
  // glyph to the caller.
  return ((*character_glyph_map_)[utf32_character] =
              font_->GetGlyphForCharacter(utf32_character)) !=
         render_tree::kInvalidGlyphIndex;
}

FontList::FontList(FontCache* font_cache, const FontListKey& font_list_key)
    : font_cache_(font_cache),
      style_(font_list_key.style),
      size_(font_list_key.size),
      is_font_metrics_set_(false),
      font_metrics_(0, 0, 0, 0),
      is_space_width_set_(false),
      space_width_(0),
      is_ellipsis_info_set_(false),
      ellipsis_width_(0) {
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

void FontList::GenerateFontRunList(const std::string& text,
                                   FontRunList* run_list) {
  run_list->clear();

  base::i18n::UTF8CharIterator iter(&text);
  scoped_refptr<render_tree::Font> font = GetPrimaryFont();

  // Walk each character in the character iterator, grabbing the proper font
  // to use. If the previous character's typeface id isn't the same as the
  // current character's font id, then the run has ended and it should be added
  // to the font run list. There is one exception to this when the length of the
  // run is 0. This means that the first font wasn't the primary font, and a run
  // hasn't started yet.
  int32 start_array_index = 0;
  for (; !iter.end(); iter.Advance()) {
    const scoped_refptr<render_tree::Font>& current_font =
        GetCharacterFont(iter.get());

    if (font->GetTypefaceId() != current_font->GetTypefaceId()) {
      int32 length = iter.array_pos() - start_array_index;
      if (length > 0) {
        run_list->push_back(FontRun(static_cast<size_t>(start_array_index),
                                    static_cast<size_t>(length), font));
      }

      font = current_font;
      start_array_index = iter.array_pos();
    }
  }

  // Add in the final run.
  int32 length = static_cast<int32>(text.size()) - start_array_index;
  if (length > 0) {
    run_list->push_back(FontRun(static_cast<size_t>(start_array_index),
                                static_cast<size_t>(length), font));
  }
}

math::RectF FontList::GetBounds(const std::string& text) {
  math::RectF text_bounds(0, 0);

  FontRunList run_list;
  GenerateFontRunList(text, &run_list);

  // Walk the font run list representing the full text, adding the rect of each
  // run into the rect representing the full text.
  for (size_t i = 0; i < run_list.size(); ++i) {
    FontRun& run = run_list[i];

    math::RectF cur_text_bounds =
        run.font->GetBounds(std::string(text, run.start_position, run.length));

    text_bounds.set_width(text_bounds.width() + cur_text_bounds.width());
    text_bounds.Union(cur_text_bounds);
  }

  return text_bounds;
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

float FontList::GetSpaceWidth() {
  // The space width is lazily generated. If it hasn't been set yet, it's time
  // to set it.
  if (!is_space_width_set_) {
    is_space_width_set_ = true;
    space_width_ = GetPrimaryFont()->GetBounds(" ").width();
  }

  return space_width_;
}

std::string FontList::GetEllipsisString() const {
  return kHorizontalEllipsisUtf8Value;
}

const scoped_refptr<render_tree::Font>& FontList::GetEllipsisFont() {
  GenerateEllipsisInfo();
  return ellipsis_font_;
}

float FontList::GetEllipsisWidth() {
  GenerateEllipsisInfo();
  return ellipsis_width_;
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
      font_list_font.set_character_glyph_map(
          font_cache_->GetFontCharacterGlyphMap(render_tree_font));
    }
  } else {
    font_list_font.set_state(state);
  }
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

  return primary_font_;
}

const scoped_refptr<render_tree::Font>& FontList::GetCharacterFont(
    int32 utf32_character) {
  // Walk the list of fonts, requesting any encountered that are in an
  // unrequested state. The first font encountered that has the character is the
  // character font.
  for (size_t i = 0; i < fonts_.size(); ++i) {
    FontListFont& font_list_font = fonts_[i];

    if (font_list_font.state() == FontListFont::kUnrequestedState) {
      RequestFont(i);
    }

    if (font_list_font.state() == FontListFont::kLoadedState &&
        font_list_font.HasGlyphForCharacter(utf32_character)) {
      return font_list_font.font();
    }
  }

  // In the case that no fonts within the font list had the requested character,
  // the typeface id supporting the character needs to be requested from the
  // font cache.
  render_tree::TypefaceId fallback_typeface_id =
      font_cache_->GetCharacterFallbackTypefaceId(utf32_character, style_,
                                                  size_);

  // Check to see if the typeface id already maps to a specific font. If it does
  // simply return that font.
  FallbackTypefaceToFontMap::iterator fallback_iterator =
      fallback_typeface_to_font_map_.find(fallback_typeface_id);
  if (fallback_iterator != fallback_typeface_to_font_map_.end()) {
    return fallback_iterator->second;
  }

  // Otherwise, request the specific font from the font cache. This is both
  // added to the map so that it will immediately be available next time and
  // returned to the caller.
  return fallback_typeface_to_font_map_[fallback_typeface_id] =
             font_cache_->GetFallbackFont(fallback_typeface_id, size_);
}

void FontList::GenerateEllipsisInfo() {
  if (!is_ellipsis_info_set_) {
    is_ellipsis_info_set_ = true;
    std::string ellipsis_string(kHorizontalEllipsisUtf8Value);
    ellipsis_font_ =
        GetCharacterFont(base::i18n::UTF8CharIterator(&ellipsis_string).get());
    ellipsis_width_ = ellipsis_font_->GetBounds(ellipsis_string).width();
  }
}

}  // namespace dom
}  // namespace cobalt
