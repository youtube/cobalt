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

#include "cobalt/renderer/rasterizer/common/utils.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/matrix_transform_node.h"
#include "cobalt/render_tree/rect_node.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace common {
namespace utils {

bool NodeCanRenderWithOpacity(render_tree::Node* node) {
  base::TypeId node_type = node->GetTypeId();

  if (node_type == base::GetTypeId<render_tree::ImageNode>() ||
      node_type == base::GetTypeId<render_tree::RectNode>()) {
    return true;
  } else if (node_type == base::GetTypeId<render_tree::MatrixTransformNode>()) {
    render_tree::MatrixTransformNode* transform_node =
        base::polymorphic_downcast<render_tree::MatrixTransformNode*>(node);
    return NodeCanRenderWithOpacity(transform_node->data().source.get());
  } else if (node_type == base::GetTypeId<render_tree::CompositionNode>()) {
    // If we are a composition of non-overlapping valid children, then we can
    // also be rendered directly onscreen. As a simplification, just check for
    // the case of 1 valid child.
    render_tree::CompositionNode* composition_node =
        base::polymorphic_downcast<render_tree::CompositionNode*>(node);
    const render_tree::CompositionNode::Children& children =
        composition_node->data().children();
    if (children.size() == 1) {
      return NodeCanRenderWithOpacity(children[0].get());
    }
  }

  return false;
}

bool IsOpaque(float opacity) {
  return opacity >= 254.5f / 255.0f;
}

bool IsTransparent(float opacity) {
  return opacity <= 0.5f / 255.0f;
}

}  // namespace utils
}  // namespace common
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
