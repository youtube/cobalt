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

#include "cobalt/render_tree/rect_shadow_node.h"

#include "cobalt/render_tree/node_visitor.h"

namespace cobalt {
namespace render_tree {

void RectShadowNode::Accept(NodeVisitor* visitor) { visitor->Visit(this); }

math::RectF RectShadowNode::GetBounds() const {
  if (data_.inset) {
    return data_.rect;
  } else {
    math::RectF shadow_bounds = data_.shadow.ToShadowBounds(data_.rect);
    shadow_bounds.Outset(data_.spread, data_.spread);
    return shadow_bounds;
  }
}

void RectShadowNode::AssertValid() const {
  if (data_.rounded_corners) {
    DCHECK(data_.rounded_corners->IsNormalized(data_.rect));
  }
}

}  // namespace render_tree
}  // namespace cobalt
