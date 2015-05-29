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
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace render_tree {

// TODO(***REMOVED***): Define the shadow.
class Shadow;

// A single line of text or a directional run as specified by Unicode
// Bidirectional Algorithm (http://www.unicode.org/reports/tr9/).
// When rendering the text node, the origin will horizontally be on the far left
// of the text, and vertically it will be on the text's baseline.  This means
// that the text bounding box may cover area above and below the TextNode's
// origin.
class TextNode : public Node {
 public:
  struct Builder {
    Builder(const std::string& text, const scoped_refptr<Font>& font,
            const ColorRGBA& color);

    // A text to draw. Guaranteed not to contain newlines.
    std::string text;

    // The font to draw the text with.
    scoped_refptr<Font> font;

    // The foreground color of the text.
    ColorRGBA color;
  };

  explicit TextNode(const Builder& builder) : data_(builder) {}
  TextNode(const std::string& text, const scoped_refptr<Font>& font,
           const ColorRGBA& color) : data_(text, font, color) {}

  void Accept(NodeVisitor* visitor) OVERRIDE;
  math::RectF GetBounds() const OVERRIDE;

  const Builder& data() const { return data_; }

 private:
  const Builder data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // RENDER_TREE_TEXT_NODE_H_
