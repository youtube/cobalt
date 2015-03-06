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

#include "cobalt/render_tree/image_node.h"

#include "cobalt/render_tree/node_visitor.h"

namespace cobalt {
namespace render_tree {

ImageNode::Builder::Builder(const scoped_refptr<Image>& source,
                            const math::SizeF& destination_size) :
    source(source), destination_size(destination_size) {}

ImageNode::ImageNode(const scoped_refptr<Image>& source)
    : data_(source, math::SizeF(source->GetWidth(), source->GetHeight())) {}

ImageNode::ImageNode(const scoped_refptr<Image>& source,
                     const math::SizeF& destination_size)
    : data_(source, destination_size) {}

void ImageNode::Accept(NodeVisitor* visitor) { visitor->Visit(this); }

}  // namespace render_tree
}  // namespace cobalt
