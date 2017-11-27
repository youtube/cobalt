// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/skia/text_shaper.h"

#include <algorithm>
#include <limits>

#include "base/i18n/char_iterator.h"
#include "base/i18n/icu_string_conversions.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/unicode/character.h"
#include "cobalt/base/unicode/character_values.h"
#include "cobalt/renderer/rasterizer/skia/glyph_buffer.h"
#include "cobalt/renderer/rasterizer/skia/harfbuzz_font.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {
namespace {

// A simple implementation of FontProvider where a single font is always used.
class SingleFontFontProvider : public render_tree::FontProvider {
 public:
  explicit SingleFontFontProvider(const scoped_refptr<render_tree::Font>& font)
      : size_(base::polymorphic_downcast<Font*>(font.get())->size()),
        font_(font) {}

  const render_tree::FontStyle& style() const override { return style_; }
  float size() const override { return size_; }

  const scoped_refptr<render_tree::Font>& GetCharacterFont(
      int32 utf32_character, render_tree::GlyphIndex* glyph_index) override {
    *glyph_index = font_->GetGlyphForCharacter(utf32_character);
    return font_;
  }

 private:
  render_tree::FontStyle style_;
  float size_;
  scoped_refptr<render_tree::Font> font_;
};

void TryAddFontToUsedFonts(Font* font,
                           render_tree::FontVector* maybe_used_fonts) {
  if (!maybe_used_fonts) {
    return;
  }

  // Verify that the font has not already been added to the used fonts, before
  // adding it to the end.
  for (int i = 0; i < maybe_used_fonts->size(); ++i) {
    if ((*maybe_used_fonts)[i] == font) {
      return;
    }
  }

  maybe_used_fonts->push_back(font);
}

bool ShouldFakeBoldText(const render_tree::FontProvider* font_provider,
                        const Font* font) {
  // A font-weight greater than 500 indicates bold if it is available. Use
  // synthetic bolding if this is a bold weight and the selected font
  // synthesizes bold.
  // https://www.w3.org/TR/css-fonts-3/#font-weight-prop
  // https://www.w3.org/TR/css-fonts-3/#font-style-matching
  if (font_provider->style().weight > 500) {
    SkAutoTUnref<SkTypeface_Cobalt> typeface(font->GetSkTypeface());
    return typeface->synthesizes_bold();
  }
  return false;
}

}  // namespace

TextShaper::TextShaper()
    : local_glyph_array_size_(0), local_text_buffer_size_(0) {}

scoped_refptr<GlyphBuffer> TextShaper::CreateGlyphBuffer(
    const char16* text_buffer, size_t text_length, const std::string& language,
    bool is_rtl, render_tree::FontProvider* font_provider) {
  math::RectF bounds;
  SkTextBlobBuilder builder;
  ShapeText(text_buffer, text_length, language, is_rtl, font_provider, &builder,
            &bounds, NULL);
  return make_scoped_refptr(new GlyphBuffer(bounds, &builder));
}

scoped_refptr<GlyphBuffer> TextShaper::CreateGlyphBuffer(
    const std::string& utf8_string,
    const scoped_refptr<render_tree::Font>& font) {
  string16 utf16_string;
  base::CodepageToUTF16(utf8_string, base::kCodepageUTF8,
                        base::OnStringConversionError::SUBSTITUTE,
                        &utf16_string);
  SingleFontFontProvider font_provider(font);
  return CreateGlyphBuffer(utf16_string.c_str(), utf16_string.size(), "en-US",
                           false, &font_provider);
}

float TextShaper::GetTextWidth(const char16* text_buffer, size_t text_length,
                               const std::string& language, bool is_rtl,
                               render_tree::FontProvider* font_provider,
                               render_tree::FontVector* maybe_used_fonts) {
  return ShapeText(text_buffer, text_length, language, is_rtl, font_provider,
                   NULL, NULL, maybe_used_fonts);
}

void TextShaper::PurgeCaches() {
  base::AutoLock lock(shaping_mutex_);
  harfbuzz_font_provider_.PurgeCaches();
}

float TextShaper::ShapeText(const char16* text_buffer, size_t text_length,
                            const std::string& language, bool is_rtl,
                            render_tree::FontProvider* font_provider,
                            SkTextBlobBuilder* maybe_builder,
                            math::RectF* maybe_bounds,
                            render_tree::FontVector* maybe_used_fonts) {
  base::AutoLock lock(shaping_mutex_);
  float total_width = 0;
  VerticalBounds vertical_bounds;
  // Only set |maybe_vertical_bounds| to a non-NULL value when |maybe_bounds| is
  // non-NULL. Otherwise, the text bounds are not being calculated.
  VerticalBounds* maybe_vertical_bounds =
      maybe_bounds ? &vertical_bounds : NULL;

  // Check for if the text contains a complex script, meaning that it requires
  // HarfBuzz. If it does, then attempt to collect the scripts. In the event
  // that this fails, fall back to the simple shaper.
  ScriptRuns script_runs;
  if (base::unicode::ContainsComplexScript(text_buffer, text_length) &&
      CollectScriptRuns(text_buffer, text_length, font_provider,
                        &script_runs)) {
    // If the direction is RTL, then reverse the script runs, so that the glyphs
    // will be added in the correct order.
    if (is_rtl) {
      std::reverse(script_runs.begin(), script_runs.end());
    }

    for (int i = 0; i < script_runs.size(); ++i) {
      ScriptRun& run = script_runs[i];
      const char16* script_run_text_buffer = text_buffer + run.start_index;

      // Check to see if the script run requires HarfBuzz. Because HarfBuzz
      // shaping is much slower than simple shaping, we only want to run
      // HarfBuzz shaping on the bare minimum possible, so any script run
      // that doesn't contain complex scripts will be simply shaped.
      if (base::unicode::ContainsComplexScript(script_run_text_buffer,
                                               run.length)) {
        ShapeComplexRun(script_run_text_buffer, run, language, is_rtl,
                        font_provider, maybe_builder, maybe_vertical_bounds,
                        maybe_used_fonts, &total_width);
      } else {
        ShapeSimpleRunWithDirection(script_run_text_buffer, run.length, is_rtl,
                                    font_provider, maybe_builder,
                                    maybe_vertical_bounds, maybe_used_fonts,
                                    &total_width);
      }
    }
  } else {
    ShapeSimpleRunWithDirection(text_buffer, text_length, is_rtl, font_provider,
                                maybe_builder, maybe_vertical_bounds,
                                maybe_used_fonts, &total_width);
  }

  // If |maybe_bounds| has been provided, then update the width of the bounds
  // with the total width of all of the shaped glyphs. The height is already
  // correct.
  if (maybe_bounds) {
    *maybe_bounds = math::RectF(0, vertical_bounds.GetY(), total_width,
                                vertical_bounds.GetHeight());
  }

  return total_width;
}

bool TextShaper::CollectScriptRuns(const char16* text_buffer,
                                   size_t text_length,
                                   render_tree::FontProvider* font_provider,
                                   ScriptRuns* runs) {
  // Initialize the next data with the first character. This allows us to avoid
  // checking for the first character case within the while loop.
  int32 next_character = base::unicode::NormalizeSpaces(
      base::i18n::UTF16CharIterator(text_buffer, text_length).get());
  render_tree::GlyphIndex next_glyph = render_tree::kInvalidGlyphIndex;
  Font* next_font = base::polymorphic_downcast<Font*>(
      font_provider->GetCharacterFont(next_character, &next_glyph).get());

  UErrorCode error_code = U_ZERO_ERROR;
  UScriptCode next_script = uscript_getScript(next_character, &error_code);
  // If we're unable to determine the script of a character, we failed to
  // successfully collect the script runs. Return failure.
  if (U_FAILURE(error_code)) {
    return false;
  }

  // Iterate for as long as the current index has not reached the end index.
  unsigned int current_index = 0;
  unsigned int end_index = text_length;
  while (current_index < end_index) {
    Font* current_font = next_font;
    UScriptCode current_script = next_script;

    // Create an iterator starting at the current index and containing the
    // remaining length.
    base::i18n::UTF16CharIterator iter(text_buffer + current_index,
                                       end_index - current_index);
    // Skip over the first character. It's already set as our current font
    // and current script.
    iter.Advance();
    size_t next_index_offset = iter.array_pos();

    for (; !iter.end(); iter.Advance(), next_index_offset = iter.array_pos()) {
      // Normalize spaces within the current character, so that we are
      // guaranteed that they will map to a consistent font and glyph.
      next_character = base::unicode::NormalizeSpaces(iter.get());
      if (next_character == base::unicode::kZeroWidthSpaceCharacter) {
        continue;
      }

      // Check for mark characters. If this is a mark character and the current
      // font contains a glyph for it, then simply add it to the current run.
      // Doing so ensures that combining character sequences will be shaped in
      // one font run if possible.
      if ((U_GET_GC_MASK(next_character) & U_GC_M_MASK) &&
          (current_font->GetGlyphForCharacter(next_character) !=
           render_tree::kInvalidGlyphIndex)) {
        continue;
      }

      next_font = base::polymorphic_downcast<Font*>(
          font_provider->GetCharacterFont(next_character, &next_glyph).get());

      next_script = uscript_getScript(next_character, &error_code);
      // If we're unable to determine the script of a character, we failed to
      // successfully collect the script runs. Return failure.
      if (U_FAILURE(error_code)) {
        return false;
      }

      // We've reached the end of the script run in two cases:
      //   1. If the characters use different fonts.
      //   2. If the characters use different scripts, the next script isn't
      //      inherited, and the next character doesn't support the current
      //      script.
      if ((current_font != next_font) ||
          ((current_script != next_script) &&
           (next_script != USCRIPT_INHERITED) &&
           (!uscript_hasScript(next_character, current_script)))) {
        break;
      }

      // If the script is inherited, then simply retain the current script.
      if (next_script == USCRIPT_INHERITED) {
        next_script = current_script;
      }
    }

    runs->push_back(ScriptRun(current_font, current_script, current_index,
                              next_index_offset));

    // Update the index where we're starting the next script run.
    current_index += next_index_offset;
  }

  return true;
}

void TextShaper::ShapeComplexRun(const char16* text_buffer,
                                 const ScriptRun& script_run,
                                 const std::string& language, bool is_rtl,
                                 render_tree::FontProvider* font_provider,
                                 SkTextBlobBuilder* maybe_builder,
                                 VerticalBounds* maybe_vertical_bounds,
                                 render_tree::FontVector* maybe_used_fonts,
                                 float* total_width) {
  TryAddFontToUsedFonts(script_run.font, maybe_used_fonts);

  hb_font_t* harfbuzz_font =
      harfbuzz_font_provider_.GetHarfBuzzFont(script_run.font);

  // Ensure that the local text buffer is large enough to hold the normalized
  // string.
  EnsureLocalTextBufferHasSize(script_run.length);

  // Normalize the spaces in the run before providing it to HarfBuzz.
  bool error = false;
  unsigned int normalized_buffer_length = 0;
  unsigned int buffer_position = 0;
  while (buffer_position < script_run.length) {
    UChar32 character;
    U16_NEXT(text_buffer, buffer_position, script_run.length, character);
    character = base::unicode::NormalizeSpacesInComplexScript(character);
    U16_APPEND(local_text_buffer_.get(), normalized_buffer_length,
               script_run.length, character, error);
  }

  // Create a HarfBuzz buffer and add the string to be shaped. The HarfBuzz
  // buffer holds our text, run information to be used by the shaping engine,
  // and the resulting glyph data.
  hb_buffer_t* buffer = hb_buffer_create();
  hb_buffer_add_utf16(buffer, (const uint16_t*)local_text_buffer_.get(),
                      normalized_buffer_length, 0, normalized_buffer_length);
  hb_buffer_set_script(buffer, hb_icu_script_to_script(script_run.script));
  hb_buffer_set_direction(buffer, is_rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
  hb_buffer_set_language(
      buffer, hb_language_from_string(language.c_str(), language.length()));

  // Shape the text.
  hb_shape(harfbuzz_font, buffer, NULL, 0);

  // Populate the run fields with the resulting glyph data in the buffer.
  unsigned int glyph_count = 0;

  hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer, &glyph_count);
  hb_glyph_position_t* hb_positions =
      hb_buffer_get_glyph_positions(buffer, NULL);

  // If |maybe_builder| has been provided, the allocate enough memory within
  // the builder for the shaped glyphs.
  const SkTextBlobBuilder::RunBuffer* run_buffer = NULL;
  if (maybe_builder) {
    SkPaint paint = script_run.font->GetSkPaint();
    paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
    paint.setFakeBoldText(ShouldFakeBoldText(font_provider, script_run.font));
    run_buffer = &(maybe_builder->allocRunPos(paint, glyph_count));
  }

  // Walk each of the shaped glyphs.
  size_t pos_index = 0;
  for (size_t i = 0; i < glyph_count; ++i) {
    DCHECK_LE(infos[i].codepoint, std::numeric_limits<uint16_t>::max());
    // If |run_buffer| has been allocated, then we're populating it with the
    // glyph and position data.
    if (run_buffer) {
      run_buffer->glyphs[i] = static_cast<uint16_t>(infos[i].codepoint);
      run_buffer->pos[pos_index++] =
          *total_width + SkFixedToScalar(hb_positions[i].x_offset);
      run_buffer->pos[pos_index++] = -SkFixedToScalar(hb_positions[i].y_offset);
    }

    // If |maybe_vertical_bounds| has been provided, then we're updating it with
    // the vertical bounds of all of the shaped glyphs.
    if (maybe_vertical_bounds) {
      const math::RectF& glyph_bounds =
          script_run.font->GetGlyphBounds(infos[i].codepoint);
      float min_y =
          glyph_bounds.y() - SkFixedToScalar(hb_positions[i].y_offset);
      float max_y = min_y + glyph_bounds.height();
      maybe_vertical_bounds->IncludeRange(min_y, max_y);
    }

    // Include the shaped glyph within the total width.
    *total_width += SkFixedToFloat(hb_positions[i].x_advance);
  }

  hb_buffer_destroy(buffer);
  hb_font_destroy(harfbuzz_font);
}

void TextShaper::ShapeSimpleRunWithDirection(
    const char16* text_buffer, size_t text_length, bool is_rtl,
    render_tree::FontProvider* font_provider, SkTextBlobBuilder* maybe_builder,
    VerticalBounds* maybe_vertical_bounds,
    render_tree::FontVector* maybe_used_fonts, float* total_width) {
  // If the text has an RTL direction and a builder was provided, then reverse
  // the text. This ensures that the glyphs will appear in the proper order
  // within the glyph buffer. The width and bounds do not rely on the direction
  // of the text, so in the case where there is no builder, reversing the text
  // is not necessary.
  if (maybe_builder && is_rtl) {
    // Ensure that the local text buffer is large enough to hold the reversed
    // string.
    EnsureLocalTextBufferHasSize(text_length);

    // Both reverse the text and replace mirror characters so that characters
    // such as parentheses will appear in the proper direction.
    bool error = false;
    unsigned int reversed_buffer_length = 0;
    unsigned int buffer_position = text_length;
    while (buffer_position > 0) {
      UChar32 character;
      U16_PREV(text_buffer, 0, buffer_position, character);
      character = u_charMirror(character);
      U16_APPEND(local_text_buffer_.get(), reversed_buffer_length, text_length,
                 character, error);
    }

    ShapeSimpleRun(local_text_buffer_.get(), reversed_buffer_length,
                   font_provider, maybe_builder, maybe_vertical_bounds,
                   maybe_used_fonts, total_width);
  } else {
    ShapeSimpleRun(text_buffer, text_length, font_provider, maybe_builder,
                   maybe_vertical_bounds, maybe_used_fonts, total_width);
  }
}

void TextShaper::ShapeSimpleRun(const char16* text_buffer, size_t text_length,
                                render_tree::FontProvider* font_provider,
                                SkTextBlobBuilder* maybe_builder,
                                VerticalBounds* maybe_vertical_bounds,
                                render_tree::FontVector* maybe_used_fonts,
                                float* total_width) {
  // If there's a builder (meaning that a glyph buffer is being generated), then
  // ensure that the local arrays are large enough to hold all of the glyphs
  // within the simple run.
  if (maybe_builder) {
    EnsureLocalGlyphArraysHaveSize(text_length);
  }

  int glyph_count = 0;
  Font* last_font = NULL;

  // Walk through each character within the run.
  for (base::i18n::UTF16CharIterator iter(text_buffer, text_length);
       !iter.end(); iter.Advance()) {
    // Retrieve the current character and normalize spaces and zero width
    // spaces before processing it.
    int32 character = base::unicode::NormalizeSpaces(iter.get());
    render_tree::GlyphIndex glyph = render_tree::kInvalidGlyphIndex;

    // If a space character is encountered, simply add to the width and continue
    // on. As an optimization, we don't add space character glyphs to the glyph
    // buffer.
    if (character == base::unicode::kSpaceCharacter) {
      *total_width += font_provider->GetCharacterFont(character, &glyph)
                          ->GetGlyphWidth(glyph);
      continue;
      // If a zero width space character is encountered, simply continue on. It
      // doesn't impact the width or shaping data.
    } else if (character == base::unicode::kZeroWidthSpaceCharacter) {
      continue;
    }

    // Look up the font and glyph for the current character.
    Font* current_font = base::polymorphic_downcast<Font*>(
        font_provider->GetCharacterFont(character, &glyph).get());

    // If there's a builder (meaning that a glyph buffer is being generated),
    // then we need to update the glyph buffer data for the new glyph.
    if (maybe_builder) {
      // If at least one glyph has previously been added and the font has
      // changed, then the current run has ended and we need to add the font run
      // into the glyph buffer.
      if (glyph_count > 0 && last_font != current_font) {
        TryAddFontToUsedFonts(last_font, maybe_used_fonts);
        AddFontRunToGlyphBuffer(font_provider, last_font, glyph_count,
                                maybe_builder);
        glyph_count = 0;
      }

      local_glyphs_[glyph_count] = glyph;
      local_positions_[glyph_count] = *total_width;
    }

    const math::RectF& glyph_bounds = current_font->GetGlyphBounds(glyph);
    *total_width += glyph_bounds.width();

    // If |maybe_vertical_bounds| has been provided, then we're updating it with
    // the vertical bounds of all of the shaped glyphs.
    if (maybe_vertical_bounds) {
      maybe_vertical_bounds->IncludeRange(glyph_bounds.y(),
                                          glyph_bounds.bottom());
    }

    ++glyph_count;
    last_font = current_font;
  }

  // If there's a builder (meaning that a glyph buffer is being generated), and
  // at least one glyph was generated, then we need to add the final font run
  // into the glyph buffer.
  if (maybe_builder && glyph_count > 0) {
    TryAddFontToUsedFonts(last_font, maybe_used_fonts);
    AddFontRunToGlyphBuffer(font_provider, last_font, glyph_count,
                            maybe_builder);
  }
}

void TextShaper::AddFontRunToGlyphBuffer(
    const render_tree::FontProvider* font_provider, const Font* font,
    const int glyph_count, SkTextBlobBuilder* builder) {
  SkPaint paint = font->GetSkPaint();
  paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
  paint.setFakeBoldText(ShouldFakeBoldText(font_provider, font));

  // Allocate space within the text blob for the glyphs and copy them into the
  // blob.
  const SkTextBlobBuilder::RunBuffer& buffer =
      builder->allocRunPosH(paint, glyph_count, 0);
  std::copy(&local_glyphs_[0], &local_glyphs_[0] + glyph_count, buffer.glyphs);
  std::copy(&local_positions_[0], &local_positions_[0] + glyph_count,
            buffer.pos);
}

void TextShaper::EnsureLocalGlyphArraysHaveSize(size_t size) {
  if (local_glyph_array_size_ < size) {
    local_glyph_array_size_ = size;
    local_glyphs_.reset(new render_tree::GlyphIndex[size]);
    local_positions_.reset(new SkScalar[size]);
  }
}

void TextShaper::EnsureLocalTextBufferHasSize(size_t size) {
  if (local_text_buffer_size_ < size) {
    local_text_buffer_size_ = size;
    local_text_buffer_.reset(new char16[size]);
  }
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
