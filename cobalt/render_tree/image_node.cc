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

#include "cobalt/render_tree/image_node.h"

#include "cobalt/render_tree/node_visitor.h"

namespace cobalt {
namespace render_tree {

ImageNode::Builder::Builder(const scoped_refptr<Image>& source)
    : Builder(source, source ? math::RectF(source->GetSize()) : math::RectF()) {
}

ImageNode::Builder::Builder(const scoped_refptr<Image>& source,
                            const math::RectF& destination_rect)
    : source(source), destination_rect(destination_rect) {}

ImageNode::Builder::Builder(const scoped_refptr<Image>& source,
                            const math::Vector2dF& offset)
    : source(source),
      destination_rect(PointAtOffsetFromOrigin(offset), source->GetSize()) {}

ImageNode::Builder::Builder(const scoped_refptr<Image>& source,
                            const math::RectF& destination_rect,
                            const math::Matrix3F& local_transform)
    : source(source),
      destination_rect(destination_rect),
      local_transform(local_transform) {}

void ImageNode::Accept(NodeVisitor* visitor) { visitor->Visit(this); }

math::RectF ImageNode::GetBounds() const { return data_.destination_rect; }

}  // namespace render_tree
}  // namespace cobalt
