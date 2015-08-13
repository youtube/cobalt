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
#include "cobalt/layout/used_style.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/render_tree/image_node.h"

namespace cobalt {
namespace layout {

using render_tree::ImageNode;

namespace {

// Default intrinsic width as defined in:
// http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width
const float kDefaultIntrinsicWidth = 300.0;
// Default intrinsic height as defined in:
// http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-height
const float kDefaultIntrinsicHeight = 150.0;

void AnimateCB(ReplacedBox::ReplaceImageCB replace_image_cb,
               ImageNode::Builder* image_node, base::Time /*time*/) {
  DCHECK(!replace_image_cb.is_null());
  DCHECK(image_node);
  image_node->source = replace_image_cb.Run();
  // TODO(***REMOVED***): Detect better when the intrinsic video size is used for the
  //   node size, and trigger a re-layout from the media element when the size
  //   changes.
  if (image_node->source && 0 == image_node->destination_size.height()) {
    image_node->destination_size = image_node->source->GetSize();
  }
}

}  // namespace

ReplacedBox::ReplacedBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions,
    const ReplaceImageCB& replace_image_cb,
    const UsedStyleProvider* used_style_provider,
    const scoped_refptr<Paragraph>& paragraph, int32 paragraph_position)
    : Box(computed_style, transitions, used_style_provider),
      replace_image_cb_(replace_image_cb),
      paragraph_(paragraph),
      text_position_(paragraph_position) {
  DCHECK(!replace_image_cb_.is_null());
}

Box::Level ReplacedBox::GetLevel() const { return kInlineLevel; }

void ReplacedBox::UpdateUsedSize(const LayoutParams& layout_params) {
  // TODO(***REMOVED***): See if we can determine and use the intrinsic element size.
  float intrinsic_width = kDefaultIntrinsicWidth;
  float intrinsic_height = kDefaultIntrinsicHeight;
  float intrinsic_ratio = kDefaultIntrinsicWidth / kDefaultIntrinsicHeight;

  UsedBoxMetrics horizontal_metrics = UsedBoxMetrics::ComputeHorizontal(
      layout_params.containing_block_size.width(), *computed_style());

  UsedBoxMetrics vertical_metrics = UsedBoxMetrics::ComputeVertical(
      layout_params.containing_block_size.height(), *computed_style());

  if (!horizontal_metrics.size && !vertical_metrics.size) {
    // If height and width both have computed values of auto, use the
    // intrinsic sizes, according to:
    // http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width
    horizontal_metrics.size = intrinsic_width;
    vertical_metrics.size = intrinsic_height;
  } else if (!horizontal_metrics.size && vertical_metrics.size) {
    // If width is auto and height is not, then use the intrinsic ratio to
    // calculate the width, according to:
    // http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width
    horizontal_metrics.size = *vertical_metrics.size * intrinsic_ratio;
  } else if (horizontal_metrics.size && !vertical_metrics.size) {
    // If height is auto and width is not, then use the intrinsic ratio to
    // calculate the height, according to:
    // http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-height
    vertical_metrics.size = *horizontal_metrics.size / intrinsic_ratio;
  }

  // TODO(***REMOVED***): Implement margins, borders, and paddings.
  set_used_width(*horizontal_metrics.size);
  set_used_height(*vertical_metrics.size);
}

scoped_ptr<Box> ReplacedBox::TrySplitAt(float /*available_width*/,
                                        bool /*allow_overflow*/) {
  return scoped_ptr<Box>();
}

void ReplacedBox::SplitBidiLevelRuns() {}

scoped_ptr<Box> ReplacedBox::TrySplitAtSecondBidiLevelRun() {
  return scoped_ptr<Box>();
}

base::optional<int> ReplacedBox::GetBidiLevel() const {
  return paragraph_->GetBidiLevel(text_position_);
}

float ReplacedBox::GetHeightAboveBaseline() const { return used_height(); }

void ReplacedBox::AddContentToRenderTree(
    render_tree::CompositionNode::Builder* composition_node_builder,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animations_map_builder) const {
  scoped_refptr<ImageNode> image_node =
      new render_tree::ImageNode(NULL, used_size());
  node_animations_map_builder->Add(image_node,
                                   base::Bind(AnimateCB, replace_image_cb_));
  composition_node_builder->AddChild(image_node, math::Matrix3F::Identity());
}

void ReplacedBox::DumpClassName(std::ostream* stream) const {
  *stream << "ReplacedBox ";
}

}  // namespace layout
}  // namespace cobalt
