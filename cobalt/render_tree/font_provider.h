// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_FONT_PROVIDER_H_
#define COBALT_RENDER_TREE_FONT_PROVIDER_H_

#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/glyph.h"

namespace cobalt {
namespace render_tree {

// The FontProvider class is an abstract base class representing a collection of
// fonts with a matching style and size, which provides fonts for any given
// character based upon what it considers to be the best match.
class FontProvider {
 public:
  // The style of the fonts contained within the collection.
  virtual const render_tree::FontStyle& style() const = 0;

  // The size of the fonts contained within the collection.
  virtual float size() const = 0;

  // Returns the font-glyph combination that the FontProvider considers to be
  // the best match for the passed in character. The returned font is guaranteed
  // to be non-NULL. However, the glyph index may be set to |kInvalidGlyphIndex|
  // if the returned font does not provide a glyph for the character.
  virtual const scoped_refptr<Font>& GetCharacterFont(
      int32 utf32_character, GlyphIndex* glyph_index) = 0;

 protected:
  virtual ~FontProvider() {}
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_FONT_PROVIDER_H_
