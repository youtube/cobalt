/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

FilterNode::Builder::Builder(const Shadow& shadow_filter,
                             const scoped_refptr<render_tree::Node>& source)
    : source(source), shadow_filter(shadow_filter) {}

FilterNode::FilterNode(const OpacityFilter& opacity_filter,
                       const scoped_refptr<render_tree::Node>& source)
    : data_(opacity_filter, source) {}

FilterNode::FilterNode(const ViewportFilter& viewport_filter,
                       const scoped_refptr<render_tree::Node>& source)
    : data_(viewport_filter, source) {}

FilterNode::FilterNode(const Shadow& shadow_filter,
                       const scoped_refptr<render_tree::Node>& source)
    : data_(shadow_filter, source) {}

void FilterNode::Accept(NodeVisitor* visitor) { visitor->Visit(this); }

math::RectF FilterNode::GetBounds() const {
  math::RectF source_bounds = data_.source->GetBounds();
  math::RectF destination_bounds;

  if (data_.viewport_filter) {
    destination_bounds =
        math::IntersectRects(source_bounds, data_.viewport_filter->viewport());
  } else {
    destination_bounds = source_bounds;
  }

  if (data_.shadow_filter) {
    math::RectF shadow_bounds =
        data_.shadow_filter->ToShadowBounds(destination_bounds);
    destination_bounds.Union(shadow_bounds);
  }

  return destination_bounds;
}

}  // namespace render_tree
}  // namespace cobalt
