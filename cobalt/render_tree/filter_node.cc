// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/render_tree/filter_node.h"
#include "cobalt/render_tree/node_visitor.h"

namespace cobalt {
namespace render_tree {

FilterNode::Builder::Builder(const scoped_refptr<render_tree::Node>& source)
    : source(source) {}

FilterNode::Builder::Builder(const OpacityFilter& opacity_filter,
                             const scoped_refptr<render_tree::Node>& source)
    : source(source), opacity_filter(opacity_filter) {}

FilterNode::Builder::Builder(const ViewportFilter& viewport_filter,
                             const scoped_refptr<render_tree::Node>& source)
    : source(source), viewport_filter(viewport_filter) {}

FilterNode::Builder::Builder(const BlurFilter& blur_filter,
                             const scoped_refptr<render_tree::Node>& source)
    : source(source), blur_filter(blur_filter) {}

FilterNode::Builder::Builder(const MapToMeshFilter& map_to_mesh_filter,
                             const scoped_refptr<render_tree::Node>& source)
    : source(source), map_to_mesh_filter(map_to_mesh_filter) {}

void FilterNode::Accept(NodeVisitor* visitor) { visitor->Visit(this); }

math::RectF FilterNode::Builder::GetBounds() const {
  if (map_to_mesh_filter) {
    // This hack is introduced because the MapToMeshFilter, which generates a
    // 3D object, does not fit well into the render_tree's existing 2D bounds
    // framework.  The 3D object will be rendered into the viewport, so it is
    // essentially always on the screen.  So, that is hereby indicated by
    // assigning it a radically large bounding rectangle.
    return math::RectF(-10000.0f, -10000.0f, 20000.0f, 20000.0f);
  }

  math::RectF source_bounds = source->GetBounds();
  math::RectF destination_bounds;

  if (viewport_filter) {
    destination_bounds =
        math::IntersectRects(source_bounds, viewport_filter->viewport());
  } else {
    destination_bounds = source_bounds;
  }

  if (blur_filter) {
    float blur_radius = blur_filter->blur_sigma() * 3;
    destination_bounds.Outset(blur_radius, blur_radius);
  }

  return destination_bounds;
}

void FilterNode::AssertValid() const {
  if (data_.viewport_filter && data_.viewport_filter->has_rounded_corners()) {
    DCHECK(data_.viewport_filter->rounded_corners().IsNormalized(
        data_.viewport_filter->viewport()));
  }
}

}  // namespace render_tree
}  // namespace cobalt
