// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/render_tree/matrix_transform_3d_node.h"

#include "cobalt/math/quad_f.h"
#include "cobalt/render_tree/node_visitor.h"

namespace cobalt {
namespace render_tree {

void MatrixTransform3DNode::Accept(NodeVisitor* visitor) {
  visitor->Visit(this);
}

math::RectF MatrixTransform3DNode::GetBounds() const {
  // At the time of this writing, the 3D transform described by the
  // MatrixTransform3DNode node is not respected by any rasterizers, and we
  // also don't currently have a way of expressing 3D bounds with the current
  // GetBounds() return type of math::RectF.  So, for now, we ignore the
  // transform and just return the unchanged bounds for our child node.
  return data_.source->GetBounds();
}

}  // namespace render_tree
}  // namespace cobalt
