// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/render_tree/text_node.h"

#include "cobalt/render_tree/node_visitor.h"

namespace cobalt {
namespace render_tree {

TextNode::Builder::Builder(
    const math::Vector2dF& offset,
    const scoped_refptr<render_tree::GlyphBuffer>& glyph_buffer,
    const ColorRGBA& color)
    : offset(offset), glyph_buffer(glyph_buffer), color(color) {}

void TextNode::Accept(NodeVisitor* visitor) { visitor->Visit(this); }

math::RectF TextNode::GetBounds() const {
  math::RectF bounds = data_.glyph_buffer->GetBounds();
  if (data_.shadows) {
    for (size_t i = 0; i < data_.shadows->size(); ++i) {
      bounds.Union(
          (*data_.shadows)[i].ToShadowBounds(data_.glyph_buffer->GetBounds()));
    }
  }
  bounds.Offset(data_.offset);
  return bounds;
}

}  // namespace render_tree
}  // namespace cobalt
