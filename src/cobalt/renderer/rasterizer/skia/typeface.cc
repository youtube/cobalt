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

#include "cobalt/renderer/rasterizer/skia/typeface.h"

#include "cobalt/renderer/rasterizer/skia/font.h"

#include "third_party/skia/include/core/SkPaint.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

SkiaTypeface::SkiaTypeface(const sk_sp<SkTypeface_Cobalt>& typeface)
    : typeface_(typeface) {
  character_glyph_thread_checker_.DetachFromThread();
}

const sk_sp<SkTypeface_Cobalt>& SkiaTypeface::GetSkTypeface() const {
  return typeface_;
}

render_tree::TypefaceId SkiaTypeface::GetId() const {
  return typeface_->uniqueID();
}

uint32 SkiaTypeface::GetEstimatedSizeInBytes() const {
  return static_cast<uint32>(typeface_->GetStreamLength());
}

scoped_refptr<render_tree::Font> SkiaTypeface::CreateFontWithSize(
    float font_size) {
  return scoped_refptr<render_tree::Font>(new Font(this, font_size));
}

render_tree::GlyphIndex SkiaTypeface::GetGlyphForCharacter(
    int32 utf32_character) {
  DCHECK(character_glyph_thread_checker_.CalledOnValidThread());
  // If the character falls within the first 256 characters (Latin-1), then
  // simply check the primary page for the glyph.
  if (utf32_character < kPrimaryPageSize) {
    // The first page glyph array is lazily allocated, so we don't use the
    // memory if it's never requested.
    if (!primary_page_character_glyphs_) {
      primary_page_character_glyphs_.reset(
          new render_tree::GlyphIndex[kPrimaryPageSize]);
      memset(&primary_page_character_glyphs_[0], 0xff,
             kPrimaryPageSize * sizeof(render_tree::GlyphIndex));
      // If it has already been allocated, then check the array for the
      // character. The unknown glyph signifies that the character's glyph has
      // not already been retrieved.
    } else if (primary_page_character_glyphs_[utf32_character] !=
               render_tree::kUnknownGlyphIndex) {
      return primary_page_character_glyphs_[utf32_character];
    }
    // Otherwise, check for the character's glyph within the map.
  } else {
    CharacterToGlyphMap::iterator glyph_iterator =
        character_to_glyph_map_.find(utf32_character);
    if (glyph_iterator != character_to_glyph_map_.end()) {
      return glyph_iterator->second;
    }
  }

  // If we reach this point, the character's glyph was not previously cached and
  // needs to be retrieved now.
  render_tree::GlyphIndex glyph = render_tree::kInvalidGlyphIndex;
  typeface_->charsToGlyphs(&utf32_character, SkTypeface::kUTF32_Encoding,
                           &glyph, 1);

  // Both cache and return the character's glyph.
  if (utf32_character < kPrimaryPageSize) {
    return primary_page_character_glyphs_[utf32_character] = glyph;
  } else {
    return character_to_glyph_map_[utf32_character] = glyph;
  }
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
