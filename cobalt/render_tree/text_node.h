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

#ifndef RENDER_TREE_TEXT_NODE_H_
#define RENDER_TREE_TEXT_NODE_H_

#include "base/compiler_specific.h"
#include "base/string_piece.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace render_tree {

// TODO(***REMOVED***): Define the color.
class Color;

// TODO(***REMOVED***): Define the font.
class Font;

// TODO(***REMOVED***): Define the shadow.
class Shadow;

// A single line of text or a directional run as specified by Unicode
// Bidirectional Algorithm (http://www.unicode.org/reports/tr9/).
class TextNode : public Node {
 public:
  // A type-safe branching.
  void Accept(NodeVisitor* visitor) OVERRIDE;

  // A text to draw. Guaranteed not to contain newlines.
  // The class does not own the text, it merely refers it from a resource pool.
  const base::StringPiece& text() const;

  // A font to draw the text with.
  // The class does not own the font face, it merely refers it from a
  // resource pool.
  const Font& font() const;

  // A text color.
  const Color& color() const;

  // A text shadow.
  const Shadow& shadow() const;

  // An extra spacing added between each glyph. Can be negative.
  float letter_spacing() const;

  // A position and size of the layed out text. Size is guaranteeed to be
  // consistent with the given font and include every glyph.
  const math::RectF& rect() const;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // RENDER_TREE_TEXT_NODE_H_
