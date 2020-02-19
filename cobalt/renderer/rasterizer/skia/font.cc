// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/skia/font.h"

#include "base/lazy_instance.h"

namespace {
const float kXHeightEstimateFactor = 0.56f;
}  // namespace

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {
namespace {

struct NonTrivialFontStaticFields {
  NonTrivialFontStaticFields() {
    // setSubpixel(true) and setLinearMetrics(true) in order to produce similar
    // behavior to the old SkPaint::setSubpixel(true) for metrics. Disable
    // glyph hinting since setLinearMetrics(true) forces unhinted linear
    // metrics, so it is not designed to be used with hinted glyphs.
    default_font.setSubpixel(true);
    default_font.setLinearMetrics(true);
    default_font.setHinting(SkFontHinting::kSlight);
  }

  SkFont default_font;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonTrivialFontStaticFields);
};

// |non_trivial_font_static_fields| will be lazily created on the first time
// it's accessed.
base::LazyInstance<NonTrivialFontStaticFields>::DestructorAtExit
    non_trivial_font_static_fields = LAZY_INSTANCE_INITIALIZER;

struct NonTrivialPaintStaticFields {
  NonTrivialPaintStaticFields() { default_paint.setAntiAlias(true); }

  SkPaint default_paint;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonTrivialPaintStaticFields);
};

// |non_trivial_paint_static_fields| will be lazily created on the first time
// it's accessed.
base::LazyInstance<NonTrivialPaintStaticFields>::DestructorAtExit
    non_trivial_paint_static_fields = LAZY_INSTANCE_INITIALIZER;

}  // namespace

Font::Font(SkiaTypeface* typeface, SkScalar size)
    : typeface_(typeface), size_(size) {
  DETACH_FROM_THREAD(glyph_bounds_thread_checker_);
}

const sk_sp<SkTypeface_Cobalt>& Font::GetSkTypeface() const {
  return typeface_->GetSkTypeface();
}

render_tree::TypefaceId Font::GetTypefaceId() const {
  return typeface_->GetId();
}

render_tree::FontMetrics Font::GetFontMetrics() const {
  SkFont font = GetSkFont();

  SkFontMetrics font_metrics;
  font.getMetrics(&font_metrics);

  // The x-height is the height of the 'x' glyph. It is used to find the visual
  // 'middle' of the font to allow vertical alignment to the middle of the font.
  // See also https://en.wikipedia.org/wiki/X-height
  float x_height;
  if (font_metrics.fXHeight) {
    x_height = font_metrics.fXHeight;
  } else {
    // If the font does not have an 'x' glyph, we need to estimate the value.
    // A good estimation  is to use 0.56 * the font ascent.
    x_height = font_metrics.fAscent * kXHeightEstimateFactor;
  }

  // In Skia, ascent is negative, while descent and leading are positive.
  return render_tree::FontMetrics(-font_metrics.fAscent, font_metrics.fDescent,
                                  font_metrics.fLeading, x_height);
}

render_tree::GlyphIndex Font::GetGlyphForCharacter(int32 utf32_character) {
  return typeface_->GetGlyphForCharacter(utf32_character);
}

const math::RectF& Font::GetGlyphBounds(render_tree::GlyphIndex glyph) {
  DCHECK_CALLED_ON_VALID_THREAD(glyph_bounds_thread_checker_);
  // Check to see if the glyph falls within the the first 256 glyphs. These
  // characters are part of the primary page and are stored within an array as
  // an optimization.
  if (glyph < kPrimaryPageSize) {
    // The first page is lazily allocated, so we don't use the memory if it's
    // never used.
    if (!primary_page_glyph_bounds_) {
      primary_page_glyph_bounds_.reset(new math::RectF[kPrimaryPageSize]);
      // If the page has already been allocated, then check for the glyph's
      // bounds having already been set. If this is the case, simply return the
      // bounds.
    } else if (primary_page_glyph_bounds_bits_[glyph]) {
      return primary_page_glyph_bounds_[glyph];
    }
    // Otherwise, check for the glyph's bounds within the map.
  } else {
    GlyphToBoundsMap::iterator map_iterator = glyph_to_bounds_map_.find(glyph);
    if (map_iterator != glyph_to_bounds_map_.end()) {
      return map_iterator->second;
    }
  }

  // If we reach this point, the glyph's bounds were not previously cached and
  // need to be calculated them now.
  SkFont font = GetSkFont();

  SkRect skia_bounds;
  float width = font.measureText(&glyph, sizeof(render_tree::GlyphIndex),
                                 SkTextEncoding::kGlyphID, &skia_bounds);

  // Both cache and return the glyph's bounds.
  if (glyph < kPrimaryPageSize) {
    primary_page_glyph_bounds_bits_.set(glyph, true);
    return primary_page_glyph_bounds_[glyph] =
               math::RectF(0, skia_bounds.top(), width, skia_bounds.height());
  } else {
    return glyph_to_bounds_map_[glyph] =
               math::RectF(0, skia_bounds.top(), width, skia_bounds.height());
  }
}

float Font::GetGlyphWidth(render_tree::GlyphIndex glyph) {
  return GetGlyphBounds(glyph).width();
}

SkFont Font::GetSkFont() const {
  SkFont font(GetDefaultSkFont());
  const sk_sp<SkTypeface>& typeface(typeface_->GetSkTypeface());
  font.setTypeface(typeface);
  font.setSize(size_);
  return font;
}

const SkFont& Font::GetDefaultSkFont() {
  return non_trivial_font_static_fields.Get().default_font;
}

const SkPaint& Font::GetDefaultSkPaint() {
  return non_trivial_paint_static_fields.Get().default_paint;
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
