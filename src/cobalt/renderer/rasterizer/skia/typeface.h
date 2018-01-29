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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_TYPEFACE_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_TYPEFACE_H_

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/typeface.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkTypeface_cobalt.h"

#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

// Describes a render_tree::Typeface using Skia objects such as SkTypeface.
// NOTE: Character glyph queries are not thread-safe and should only occur from
// a single thread. However, the typeface can be created on a different thread
// than the thread making the character glyph queries.
class SkiaTypeface : public render_tree::Typeface {
 public:
  explicit SkiaTypeface(const sk_sp<SkTypeface_Cobalt>& typeface);

  const sk_sp<SkTypeface_Cobalt>& GetSkTypeface() const;

  // From render_tree::Typeface

  // Returns the typeface's id, which is guaranteed to be unique among the
  // typefaces registered with the resource provider.
  render_tree::TypefaceId GetId() const override;

  // Returns a size estimate for this typeface in bytes.
  uint32 GetEstimatedSizeInBytes() const override;

  // Creates a font using this typeface, with the font's size set to the passed
  // in value.
  scoped_refptr<render_tree::Font> CreateFontWithSize(float font_size) override;

  // Returns an index to the glyph that the typeface provides for a given UTF-32
  // unicode character. If the character is unsupported, then it returns
  // kInvalidGlyphIndex. The results are cached to speed up subsequent requests
  // for the same character.
  render_tree::GlyphIndex GetGlyphForCharacter(int32 utf32_character) override;

 private:
  // Usually covers Latin-1 in a single page.
  static const int kPrimaryPageSize = 256;
  typedef base::hash_map<int32, render_tree::GlyphIndex> CharacterToGlyphMap;

  // The underlying SkTypeface that was used to create this typeface.
  sk_sp<SkTypeface_Cobalt> typeface_;

  // The glyphs for characters are lazily computed and cached to speed up later
  // lookups. The page containing indices 0-255 is optimized within an array.
  // Thread checking is used to used to ensure that they are only accessed and
  // modified on a single thread.
  scoped_array<render_tree::GlyphIndex> primary_page_character_glyphs_;
  CharacterToGlyphMap character_to_glyph_map_;
  base::ThreadChecker character_glyph_thread_checker_;
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_TYPEFACE_H_
