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

#ifndef COBALT_RENDER_TREE_TYPEFACE_H_
#define COBALT_RENDER_TREE_TYPEFACE_H_

#include "base/memory/ref_counted.h"
#include "cobalt/render_tree/glyph.h"

namespace cobalt {
namespace render_tree {

class Font;

typedef uint32_t TypefaceId;

// The Typeface class is an abstract base class representing an implementation
// specific typeface object. While it has no associated size, it supports the
// creation of concrete Font objects at any requested size. The specific glyph
// that the typeface provides for each UTF-32 character is queryable.
// NOTE: Typeface objects are not immutable and offer no thread safety
// guarantees. While Typeface is intended to assist in the creation of
// GlyphBuffer objects, only GlyphBuffer objects, which are immutable and
// thread-safe, should be accessed on multiple threads.
class Typeface : public base::RefCounted<Typeface> {
 public:
  // Returns the typeface's id, which is guaranteed to be unique among the
  // typefaces registered with the resource provider.
  virtual TypefaceId GetId() const = 0;

  // Returns a size estimate for this typeface in bytes. While the derived class
  // can potentially return an exact value, there is no guarantee that this will
  // be the case.
  virtual uint32 GetEstimatedSizeInBytes() const = 0;

  // Returns a newly created font using this typeface, with the font size set to
  // the passed in value.
  virtual scoped_refptr<Font> CreateFontWithSize(float font_size) = 0;

  // Returns an index to the glyph that the typeface provides for a given UTF-32
  // unicode character. If the character is unsupported, then it returns
  // kInvalidGlyphIndex.
  virtual GlyphIndex GetGlyphForCharacter(int32 utf32_character) = 0;

 protected:
  virtual ~Typeface() {}

  // Allow the reference counting system access to our destructor.
  friend class base::RefCounted<Typeface>;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_TYPEFACE_H_
