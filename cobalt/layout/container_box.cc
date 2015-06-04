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

#include "cobalt/layout/container_box.h"

namespace cobalt {
namespace layout {

ContainerBox::ContainerBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions,
    const UsedStyleProvider* used_style_provider)
    : Box(computed_style, transitions, used_style_provider) {}

ContainerBox::~ContainerBox() {}

void ContainerBox::AddPositionedChild(Box* child_box) {
  DCHECK(child_box->IsPositioned());
  positioned_child_boxes_.push_back(child_box);
}

void ContainerBox::UpdateUsedSizeOfPositionedChildren(
    const LayoutParams& layout_params) {
  // Compute sizes for positioned children with invalidated sizes.
  for (ChildBoxes::const_iterator iter = positioned_child_boxes_.begin();
       iter != positioned_child_boxes_.end(); ++iter) {
    Box* child_box = *iter;

    child_box->UpdateUsedSizeIfInvalid(layout_params);
  }
}

void ContainerBox::AddPositionedChildrenToRenderTree(
    render_tree::CompositionNode::Builder* composition_node_builder,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animations_map_builder) const {
  // Add all positioned children to the render tree.
  for (ChildBoxes::const_iterator iter = positioned_child_boxes_.begin();
       iter != positioned_child_boxes_.end(); ++iter) {
    Box* child_box = *iter;
    child_box->AddToRenderTree(composition_node_builder,
                               node_animations_map_builder);
  }
}

}  // namespace layout
}  // namespace cobalt
