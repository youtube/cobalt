/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_FONT_H_
#define COBALT_RENDER_TREE_FONT_H_

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/glyph.h"

namespace cobalt {
namespace render_tree {

// Contains metrics common for all glyphs in the font.
class FontMetrics {
 public:
  FontMetrics(float ascent, float descent, float leading, float x_height)
      : ascent_(ascent),
        descent_(descent),
        leading_(leading),
        x_height_(x_height) {}
  float ascent() const { return ascent_; }
  float descent() const { return descent_; }
  float leading() const { return leading_; }
  float x_height() const { return x_height_; }
  float em_box_height() const { return ascent_ + descent_ + leading_; }
  float baseline_offset_from_top() const { return ascent_ + leading_ / 2; }

 private:
  // The recommended distance above the baseline.
  float ascent_;
  // The recommended distance below the baseline.
  float descent_;
  // The recommended distance to add between lines of text.
  float leading_;
  // The x-height, aka the height of the 'x' glyph, used for centering.
  // See also https://en.wikipedia.org/wiki/X-height
  float x_height_;
};

// Used as a parameter to GetLocalFont() and GetCharacterFallbackFont() to
// describe the font style the caller is seeking.
struct FontStyle {
  enum Weight {
    kThinWeight = 100,
    kExtraLightWeight = 200,
    kLightWeight = 300,
    kNormalWeight = 400,
    kMediumWeight = 500,
    kSemiBoldWeight = 600,
    kBoldWeight = 700,
    kExtraBoldWeight = 800,
    kBlackWeight = 900
  };

  enum Slant {
    kUprightSlant,
    kItalicSlant,
  };

  FontStyle() : weight(kNormalWeight), slant(kUprightSlant) {}
  FontStyle(Weight font_weight, Slant font_slant)
      : weight(font_weight), slant(font_slant) {}

  Weight weight;
  Slant slant;
};

typedef uint32_t TypefaceId;

// The Font class is an abstract base class representing all information
// required by the rasterizer to determine the font metrics for a given
// glyph.  Typically this implies that a font typeface, size, and style must at
// least be described when instantiating a concrete Font derived class.  Since
// Font objects may be created in the frontend, but must be accessed by the
// rasterizer, it is expected that they will be downcast again to a
// rasterizer-specific type through base::Downcast().
class Font : public base::RefCounted<Font> {
 public:
  // Returns the font typeface's id, which is guaranteed to be unique among the
  // typefaces registered with the font's resource provider.
  virtual TypefaceId GetTypefaceId() const = 0;

  // Returns the metrics common for all glyphs in the font. Used to calculate
  // the recommended line height and spacing between the lines.
  virtual FontMetrics GetFontMetrics() const = 0;

  // Returns a size estimate for this font. While the derived class can
  // potentially return an exact value, there is no guarantee that this will be
  // the case.
  virtual uint32 GetEstimatedSizeInBytes() const = 0;

  // Returns a clone of the font, with the font size of the clone set to the
  // passed in value.
  virtual scoped_refptr<Font> CloneWithSize(float font_size) const = 0;

  // Returns an index to the glyph that the font provides for a given UTF-32
  // unicode character. If the character is unsupported, then it returns
  // kInvalidGlyphIndex.
  virtual GlyphIndex GetGlyphForCharacter(int32 utf32_character) const = 0;

  // Returns the bounding box for a given glyph. While left is always zero,
  // note that since the glyph is output with the origin vertically on the
  // baseline, the top of the bounding box will likely be non-zero and is used
  // to indicate the offset of the glyph bounding box from the origin.  The
  // return value is given in units of pixels.
  // NOTE: GetGlyphBounds is not guaranteed to be thread-safe and should only
  // be called from a single thread.
  virtual const math::RectF& GetGlyphBounds(GlyphIndex glyph) const = 0;

  // Returns the width of a given glyph. The return value is given in units of
  // pixels.
  // NOTE: GetGlyphWidth is not guaranteed to be thread-safe and should only
  // be called from a single thread.
  virtual float GetGlyphWidth(GlyphIndex glyph) const = 0;

 protected:
  virtual ~Font() {}

  // Allow the reference counting system access to our destructor.
  friend class base::RefCounted<Font>;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_FONT_H_
