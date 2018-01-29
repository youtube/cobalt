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

#include "cobalt/renderer/rasterizer/skia/harfbuzz_font.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <map>
#include <utility>

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/render_tree/glyph.h"

#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

namespace {

// Deletes the object at the given pointer after casting it to the given type.
template <typename Type>
void DeleteByType(void* data) {
  Type* typed_data = reinterpret_cast<Type*>(data);
  delete typed_data;
}

template <typename Type>
void DeleteArrayByType(void* data) {
  Type* typed_data = reinterpret_cast<Type*>(data);
  delete[] typed_data;
}

// Outputs the |width| and |extents| of the glyph with index |codepoint| in
// |paint|'s font.
void GetGlyphWidthAndExtents(Font* skia_font, hb_codepoint_t codepoint,
                             hb_position_t* width,
                             hb_glyph_extents_t* extents) {
  DCHECK_LE(codepoint, std::numeric_limits<uint16_t>::max());

  uint16_t glyph = static_cast<uint16_t>(codepoint);
  const math::RectF& bounds = skia_font->GetGlyphBounds(glyph);

  if (width) {
    *width = SkScalarToFixed(bounds.width());
  }
  if (extents) {
    // Invert y-axis because Skia is y-grows-down but we set up HarfBuzz to be
    // y-grows-up.
    extents->x_bearing = SkScalarToFixed(bounds.x());
    extents->y_bearing = SkScalarToFixed(-bounds.y());
    extents->width = SkScalarToFixed(bounds.width());
    extents->height = SkScalarToFixed(-bounds.height());
  }
}

// Writes the |glyph| index for the given |unicode| code point. Returns whether
// the glyph exists, i.e. it is not a missing glyph.
hb_bool_t GetGlyph(hb_font_t* font, void* data, hb_codepoint_t unicode,
                   hb_codepoint_t variation_selector, hb_codepoint_t* glyph,
                   void* user_data) {
  Font* skia_font = reinterpret_cast<Font*>(data);
  *glyph = skia_font->GetGlyphForCharacter(unicode);
  return !!*glyph;
}

// Returns the horizontal advance value of the |glyph|.
hb_position_t GetGlyphHorizontalAdvance(hb_font_t* font, void* data,
                                        hb_codepoint_t glyph, void* user_data) {
  Font* skia_font = reinterpret_cast<Font*>(data);
  hb_position_t advance = 0;

  GetGlyphWidthAndExtents(skia_font, glyph, &advance, 0);
  return advance;
}

hb_bool_t GetGlyphHorizontalOrigin(hb_font_t* font, void* data,
                                   hb_codepoint_t glyph, hb_position_t* x,
                                   hb_position_t* y, void* user_data) {
  // Just return true, like the HarfBuzz-FreeType implementation.
  return true;
}

hb_position_t GetGlyphKerning(Font* font_data, hb_codepoint_t first_glyph,
                              hb_codepoint_t second_glyph) {
  const sk_sp<SkTypeface_Cobalt>& typeface(font_data->GetSkTypeface());
  const uint16_t glyphs[2] = {static_cast<uint16_t>(first_glyph),
                              static_cast<uint16_t>(second_glyph)};
  int32_t kerning_adjustments[1] = {0};

  if (!typeface->getKerningPairAdjustments(glyphs, 2, kerning_adjustments)) {
    return 0;
  }

  SkScalar size = font_data->size();
  SkScalar upm = SkIntToScalar(typeface->getUnitsPerEm());
  // Similar to
  // https://github.com/flutter/engine/pull/3430/commits/e40447c475cdc2154139c39aecf3d40a5e681f72
  return SkScalarToFixed(kerning_adjustments[0] * size / upm);
}

hb_position_t GetGlyphHorizontalKerning(hb_font_t* font, void* data,
                                        hb_codepoint_t left_glyph,
                                        hb_codepoint_t right_glyph,
                                        void* user_data) {
  Font* skia_font = reinterpret_cast<Font*>(data);
  return GetGlyphKerning(skia_font, left_glyph, right_glyph);
}

hb_position_t GetGlyphVerticalKerning(hb_font_t* font, void* data,
                                      hb_codepoint_t top_glyph,
                                      hb_codepoint_t bottom_glyph,
                                      void* user_data) {
  // We don't support vertical text currently.
  return 0;
}

// Writes the |extents| of |glyph|.
hb_bool_t GetGlyphExtents(hb_font_t* font, void* data, hb_codepoint_t glyph,
                          hb_glyph_extents_t* extents, void* user_data) {
  Font* skia_font = reinterpret_cast<Font*>(data);
  GetGlyphWidthAndExtents(skia_font, glyph, 0, extents);
  return true;
}

class FontFuncs {
 public:
  FontFuncs() : font_funcs_(hb_font_funcs_create()) {
    hb_font_funcs_set_glyph_func(font_funcs_, GetGlyph, 0, 0);
    hb_font_funcs_set_glyph_h_advance_func(font_funcs_,
                                           GetGlyphHorizontalAdvance, 0, 0);
    hb_font_funcs_set_glyph_h_kerning_func(font_funcs_,
                                           GetGlyphHorizontalKerning, 0, 0);
    hb_font_funcs_set_glyph_h_origin_func(font_funcs_, GetGlyphHorizontalOrigin,
                                          0, 0);
    hb_font_funcs_set_glyph_v_kerning_func(font_funcs_, GetGlyphVerticalKerning,
                                           0, 0);
    hb_font_funcs_set_glyph_extents_func(font_funcs_, GetGlyphExtents, 0, 0);
    hb_font_funcs_make_immutable(font_funcs_);
  }

  ~FontFuncs() { hb_font_funcs_destroy(font_funcs_); }

  hb_font_funcs_t* get() { return font_funcs_; }

 private:
  hb_font_funcs_t* font_funcs_;

  DISALLOW_COPY_AND_ASSIGN(FontFuncs);
};

base::LazyInstance<FontFuncs>::Leaky g_font_funcs = LAZY_INSTANCE_INITIALIZER;

// Returns the raw data of the font table |tag|.
hb_blob_t* GetFontTable(hb_face_t* face, hb_tag_t tag, void* user_data) {
  auto* skia_face = reinterpret_cast<sk_sp<SkTypeface_Cobalt>*>(user_data);
  SkTypeface_Cobalt* typeface = skia_face->get();

  const size_t table_size = typeface->getTableSize(tag);
  if (!table_size) {
    return 0;
  }

  scoped_array<char> buffer(new char[table_size]);
  if (!buffer) {
    return 0;
  }
  size_t actual_size = typeface->getTableData(tag, 0, table_size, buffer.get());
  if (table_size != actual_size) {
    return 0;
  }

  char* buffer_raw = buffer.release();
  return hb_blob_create(buffer_raw, table_size, HB_MEMORY_MODE_WRITABLE,
                        buffer_raw, DeleteArrayByType<char>);
}

void ResetSkiaFace(void* data) {
  auto* skia_face = reinterpret_cast<sk_sp<SkTypeface_Cobalt>*>(data);
  skia_face->reset();
}

}  // namespace

HarfBuzzFontProvider::HarfBuzzFace::HarfBuzzFace() : face_(NULL) {}

HarfBuzzFontProvider::HarfBuzzFace::~HarfBuzzFace() {
  if (face_) {
    hb_face_destroy(face_);
  }
}

void HarfBuzzFontProvider::HarfBuzzFace::Init(
    const sk_sp<SkTypeface_Cobalt>& skia_face) {
  DCHECK(!typeface_);
  typeface_ = skia_face;
  face_ = hb_face_create_for_tables(GetFontTable, &typeface_, ResetSkiaFace);
  DCHECK(face_);
}

hb_face_t* HarfBuzzFontProvider::HarfBuzzFace::get() {
  return face_;
}

hb_font_t* HarfBuzzFontProvider::GetHarfBuzzFont(Font* skia_font) {
  // Retrieve the typeface from the cache. In the case where it does not already
  // exist, it will be NULL and we must create it.
  HarfBuzzFace& face = face_cache_[skia_font->GetTypefaceId()];
  if (face.get() == NULL) {
    const sk_sp<SkTypeface_Cobalt>& typeface = skia_font->GetSkTypeface();
    face.Init(typeface);
  }

  hb_font_t* harfbuzz_font = hb_font_create(face.get());
  const int scale = SkScalarToFixed(skia_font->size());
  hb_font_set_scale(harfbuzz_font, scale, scale);
  hb_font_set_funcs(harfbuzz_font, g_font_funcs.Get().get(), skia_font, NULL);
  hb_font_make_immutable(harfbuzz_font);
  return harfbuzz_font;
}

void HarfBuzzFontProvider::PurgeCaches() { face_cache_.clear(); }

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
