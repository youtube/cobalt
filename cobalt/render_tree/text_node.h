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

#ifndef COBALT_RENDER_TREE_TEXT_NODE_H_
#define COBALT_RENDER_TREE_TEXT_NODE_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/string_piece.h"
#include "cobalt/base/type_id.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/math/vector2d_f.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/glyph_buffer.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/shadow.h"

namespace cobalt {
namespace render_tree {

// A single line of text or a directional run as specified by Unicode
// Bidirectional Algorithm (http://www.unicode.org/reports/tr9/).
// When rendering the text node, the origin will horizontally be on the far left
// of the text, and vertically it will be on the text's baseline.  This means
// that the text bounding box may cover area above and below the TextNode's
// origin.
class TextNode : public Node {
 public:
  struct Builder {
    Builder(const math::Vector2dF& offset,
            const scoped_refptr<GlyphBuffer>& glyph_buffer,
            const ColorRGBA& color);

    bool operator==(const Builder& other) const {
      return offset == other.offset && glyph_buffer == other.glyph_buffer &&
             color == other.color && shadows == other.shadows;
    }

    math::Vector2dF offset;

    // All of the glyph data needed to render the text.
    scoped_refptr<GlyphBuffer> glyph_buffer;

    // The foreground color of the text.
    ColorRGBA color;

    // Shadows to be applied under the text.  These will be drawn in
    // back-to-front order, so the last shadow will be on the bottom.
    base::optional<std::vector<Shadow> > shadows;
  };

  explicit TextNode(const Builder& builder) : data_(builder) {}
  TextNode(const math::Vector2dF& offset,
           const scoped_refptr<GlyphBuffer>& glyph_buffer,
           const ColorRGBA& color)
      : data_(offset, glyph_buffer, color) {}

  void Accept(NodeVisitor* visitor) override;
  math::RectF GetBounds() const override;

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<TextNode>();
  }

  const Builder& data() const { return data_; }

 private:
  const Builder data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_TEXT_NODE_H_
