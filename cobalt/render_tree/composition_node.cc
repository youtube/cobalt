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

#include "cobalt/render_tree/composition_node.h"

#include "cobalt/render_tree/node_visitor.h"

namespace cobalt {
namespace render_tree {

void CompositionNode::Builder::AddChild(const scoped_refptr<Node>& node) {
  children_.push_back(node);
}

void CompositionNode::Accept(NodeVisitor* visitor) { visitor->Visit(this); }

math::RectF CompositionNode::GetBounds() const { return cached_bounds_; }

math::RectF CompositionNode::ComputeBounds() const {
  math::RectF bounds;

  // Take the union of the bounding rectangle for all child nodes, and use that
  // as our bounding rectangle.
  for (Children::const_iterator iter = data_.children().begin();
       iter != data_.children().end(); ++iter) {
    bounds.Union((*iter)->GetBounds());
  }

  bounds.Offset(data_.offset());

  return bounds;
}

}  // namespace render_tree
}  // namespace cobalt
