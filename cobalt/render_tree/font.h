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

#ifndef RENDER_TREE_FONT_H_
#define RENDER_TREE_FONT_H_

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace render_tree {

// Contains metrics common for all glyphs in the font.
struct FontMetrics {
  FontMetrics(float ascent, float descent, float leading, float x_height)
      : ascent(ascent),
        descent(descent),
        leading(leading),
        x_height(x_height) {}

  // The recommended distance above the baseline.
  float ascent;
  // The recommended distance below the baseline.
  float descent;
  // The recommended distance to add between lines of text.
  float leading;
  // The x-height, aka the height of the 'x' glyph, used for centering.
  // See also https://en.wikipedia.org/wiki/X-height
  float x_height;
};

// Used as a parameter to GetSystemFont() to describe the font style the
// caller is seeking.
enum FontStyle {
  kNormal,
  kBold,
  kItalic,
  kBoldItalic,
};

// The Font class is an abstract base class representing all information
// required by the rasterizer to determine the font metrics for a given
// string of text.  Typically this implies that a font typeface, size, and
// style must at least be described when instantiating a concrete Font derived
// class.  Since Font objects may be created in the frontend, but must be
// accessed by the rasterizer, it is expected that they will be downcast again
// to a rasterizer-specific type through base::Downcast().
class Font : public base::RefCountedThreadSafe<Font> {
 public:
  // Returns the bounding box for a given text string.  The size is guaranteeed
  // to be consistent with the given font and include every glyph.  Note that
  // since text is output with the origin horizontally on the left and
  // vertically on the baseline, the left and top of the bounding box will
  // likely be non-zero and are used to indicate the offset of the text bounding
  // box from the origin.  The return value is given in units of pixels.
  virtual math::RectF GetBounds(const std::string& text) const = 0;

  // Returns the metrics common for all glyphs in the font. Used to calculate
  // the recommended line height and spacing between the lines.
  virtual FontMetrics GetFontMetrics() const = 0;

 protected:
  virtual ~Font() {}

  // Allow the reference counting system access to our destructor.
  friend class base::RefCountedThreadSafe<Font>;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // RENDER_TREE_FONT_H_
