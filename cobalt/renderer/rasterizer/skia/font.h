// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_FONT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_FONT_H_

#include <bitset>

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/renderer/rasterizer/skia/typeface.h"

#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

// Describes a render_tree::Font using Skia font description objects such as
// SkTypeface.
// NOTE: Glyph queries are not thread-safe and should only occur from a single
// thread. However, the font can be created on a different thread than the
// thread making the glyph queries.
class Font : public render_tree::Font {
 public:
  Font(SkiaTypeface* typeface, SkScalar size);

  // Returns the contained SkTypeface_Cobalt object, which has its reference
  // count incremented.
  // NOTE: The caller is responsible for decrementing the reference count after
  // finishing with the object.
  SkTypeface_Cobalt* GetSkTypeface() const;

  // Returns the pixel size described by this font.
  SkScalar size() const { return size_; }

  // Returns the font's typeface id, which is guaranteed to be unique among the
  // typefaces registered with the font's resource provider.
  render_tree::TypefaceId GetTypefaceId() const override;

  // Invokes Skia to determine the font metrics common for all glyphs in the
  // font.
  render_tree::FontMetrics GetFontMetrics() const override;

  // Invokes Skia to retrieve the index of the glyph that the typeface provides
  // for the given UTF-32 unicode character.
  render_tree::GlyphIndex GetGlyphForCharacter(int32 utf32_character) override;

  // Invokes Skia to determine the bounds of the given glyph if it were to be
  // rendered using this particular font. The results are cached to speed up
  // subsequent requests for the same glyph.
  const math::RectF& GetGlyphBounds(render_tree::GlyphIndex glyph) override;

  // Invokes Skia to determine the width of the given glyph if it were to be
  // rendered using this particular font. The results are cached to speed up
  // subsequent requests for the same glyph.
  float GetGlyphWidth(render_tree::GlyphIndex glyph) override;

  // Returns a SkPaint setup for rendering text with this font.  Clients
  // are free to customize the SkPaint further after obtaining it, if they
  // wish.
  SkPaint GetSkPaint() const;

  // Returns a static SkPaint with the default flags enabled.
  static const SkPaint& GetDefaultSkPaint();

 private:
  // Usually covers Latin-1 in a single page.
  static const int kPrimaryPageSize = 256;
  typedef base::hash_map<render_tree::GlyphIndex, math::RectF> GlyphToBoundsMap;

  // The SkiaTypeface that was used to create this font.
  scoped_refptr<SkiaTypeface> typeface_;

  // Size of the text in pixels.
  SkScalar size_;

  // The bounds for glyphs are lazily computed and cached to speed up later
  // lookups. The page containing indices 0-255 is optimized within an array.
  // Thread checking is used to used to ensure that they are only accessed and
  // modified on a single thread.
  std::bitset<kPrimaryPageSize> primary_page_glyph_bounds_bits_;
  scoped_array<math::RectF> primary_page_glyph_bounds_;
  GlyphToBoundsMap glyph_to_bounds_map_;
  base::ThreadChecker glyph_bounds_thread_checker_;
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_FONT_H_
