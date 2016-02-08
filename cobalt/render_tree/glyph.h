/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_GLYPH_H_
#define COBALT_RENDER_TREE_GLYPH_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace cobalt {
namespace render_tree {

typedef uint16_t GlyphIndex;
static const GlyphIndex kInvalidGlyphIndex = 0;
static const GlyphIndex kUnknownGlyphIndex = 0xffff;

// The CharacterGlyphMap class tracks character to glyph mappings. It provides
// a 256 glyph array for the primary page (Latin-1), so that glyphs in that
// range can be rapidly retrieved. All other glyphs are looked up within a map.
class CharacterGlyphMap : public base::RefCounted<CharacterGlyphMap> {
 public:
  typedef std::map<int32, GlyphIndex> CharacterToGlyphs;

  CharacterGlyphMap();

  GlyphIndex& GetCharacterGlyph(int32 character);

 private:
  // Usually covers Latin-1 in a single page.
  static const int kPrimaryPageSize = 256;

  // Optimize for the page that contains glyph indices 0-255.
  scoped_array<GlyphIndex> primary_page_glyphs_;

  // The glyphs for characters outside of the primary page are lazily added the
  // first time they are requested.
  CharacterToGlyphs character_to_glyphs_;

  // Allow the reference counting system access to our destructor.
  friend class base::RefCounted<CharacterGlyphMap>;
};


}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_GLYPH_H_
