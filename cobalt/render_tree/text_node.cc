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

#include "cobalt/render_tree/text_node.h"

#include "cobalt/render_tree/node_visitor.h"

namespace cobalt {
namespace render_tree {

TextNode::Builder::Builder(const std::string& text,
                           const scoped_refptr<Font>& font,
                           const ColorRGBA& color)
    : text(text), font(font), color(color) {}

void TextNode::Accept(NodeVisitor* visitor) { visitor->Visit(this); }

math::RectF TextNode::GetBounds() const {
  const math::RectF text_bounds = data_.font->GetBounds(data_.text);
  if (!data_.shadows) {
    return text_bounds;
  } else {
    math::RectF bounds = text_bounds;
    for (size_t i = 0; i < data_.shadows->size(); ++i) {
      bounds.Union((*data_.shadows)[i].ToShadowBounds(text_bounds));
    }

    return bounds;
  }
}

}  // namespace render_tree
}  // namespace cobalt
