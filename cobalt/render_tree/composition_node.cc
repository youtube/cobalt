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

#include "cobalt/render_tree/composition_node.h"

#include "cobalt/render_tree/node_visitor.h"

namespace cobalt {
namespace render_tree {

void CompositionNode::Builder::AddChild(const scoped_refptr<Node>& node,
                                        const math::Matrix3F& transform) {
  composed_children_->push_back(ComposedChild(node, transform));
}

void CompositionNode::Accept(NodeVisitor* visitor) { visitor->Visit(this); }

}  // namespace render_tree
}  // namespace cobalt
