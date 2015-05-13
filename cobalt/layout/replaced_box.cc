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

#include "cobalt/layout/replaced_box.h"

#include "base/bind.h"
#include "base/logging.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/render_tree/image_node.h"

namespace cobalt {
namespace layout {

using render_tree::ImageNode;

namespace {

void AnimateCB(ReplacedBox::ReplaceImageCB replace_image_cb,
               ImageNode::Builder* image_node, base::Time /*time*/) {
  DCHECK(!replace_image_cb.is_null());
  DCHECK(image_node);
  image_node->source = replace_image_cb.Run();
  // TODO(***REMOVED***) : Properly set the size of the image node.
  if (image_node->source) {
    image_node->destination_size = image_node->source->GetSize();
  }
}

}  // namespace

ReplacedBox::ReplacedBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions,
    const ReplaceImageCB& replace_image_cb)
    : Box(computed_style, transitions), replace_image_cb_(replace_image_cb) {
  DCHECK(!replace_image_cb_.is_null());
}

Box::Level ReplacedBox::GetLevel() const { return kInlineLevel; }

void ReplacedBox::Layout(const LayoutParams& /*layout_params*/) {
  // TODO(***REMOVED***) : Properly set the size of the image box from its style.
  used_frame().set_width(1280);
  used_frame().set_height(720);
}

scoped_ptr<Box> ReplacedBox::TrySplitAt(float /*available_width*/) {
  return scoped_ptr<Box>();
}

float ReplacedBox::GetHeightAboveBaseline() const {
  return used_frame().height();
}

void ReplacedBox::AddContentToRenderTree(
    render_tree::CompositionNode::Builder* composition_node_builder,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animations_map_builder) const {
  scoped_refptr<ImageNode> image_node = new render_tree::ImageNode(NULL);
  node_animations_map_builder->Add(image_node,
                                   base::Bind(AnimateCB, replace_image_cb_));
  composition_node_builder->AddChild(image_node, math::Matrix3F::Identity());
}

void ReplacedBox::DumpClassName(std::ostream* stream) const {
  *stream << "ReplacedBox ";
}

}  // namespace layout
}  // namespace cobalt
