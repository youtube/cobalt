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
    const UsedStyleProvider* used_style_provider)
    : Box(computed_style, transitions, used_style_provider),
      replace_image_cb_(replace_image_cb) {
  DCHECK(!replace_image_cb_.is_null());
}

Box::Level ReplacedBox::GetLevel() const { return kInlineLevel; }

namespace {

// Calculate the replaced box width, as defined in:
//   http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width
//   http://www.w3.org/TR/CSS21/visudet.html#block-replaced-width
//   http://www.w3.org/TR/CSS21/visudet.html#inlineblock-replaced-width
// TODO(***REMOVED***): Implement correct behaviour for absolutely positioned elements:
//   http://www.w3.org/TR/CSS21/visudet.html#abs-replaced-width
class ReplacedBoxUsedWidthProvider : public UsedWidthProvider {
 public:
  ReplacedBoxUsedWidthProvider(float containing_block_height,
                               float intrinsic_width);

  void VisitAuto() OVERRIDE;

  // This is used to determine if the width must be calculated from the height
  // and ratio.
  bool width_is_auto() const { return width_is_auto_; }

 private:
  float intrinsic_width_;
  bool width_is_auto_;
};

ReplacedBoxUsedWidthProvider::ReplacedBoxUsedWidthProvider(
    float containing_block_width, float intrinsic_width)
    : UsedWidthProvider(containing_block_width),
      intrinsic_width_(intrinsic_width),
      width_is_auto_(false) {}

void ReplacedBoxUsedWidthProvider::VisitAuto() {
  width_is_auto_ = true;
  // Note: This value will be ignored if the height is not auto.
  set_used_width(intrinsic_width_);
}

// Calculate the replaced box height, as defined in:
//   http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-height
// TODO(***REMOVED***): Implement correct behaviour for absolutely positioned elements:
//   http://www.w3.org/TR/CSS21/visudet.html#abs-replaced-height
class ReplacedBoxUsedHeightProvider : public UsedHeightProvider {
 public:
  ReplacedBoxUsedHeightProvider(float containing_block_height,
                                float intrinsic_height);

  void VisitAuto() OVERRIDE;

  // This is used to determine if the height must be calculated from the width
  // and ratio.
  bool height_is_auto() const { return height_is_auto_; }

 private:
  float intrinsic_height_;
  bool height_is_auto_;
};

ReplacedBoxUsedHeightProvider::ReplacedBoxUsedHeightProvider(
    float containing_block_height, float intrinsic_height)
    : UsedHeightProvider(containing_block_height),
      intrinsic_height_(intrinsic_height),
      height_is_auto_(false) {}

void ReplacedBoxUsedHeightProvider::VisitAuto() {
  height_is_auto_ = true;
  // Note: This value will be ignored if the width is not auto.
  set_used_height(intrinsic_height_);
}

}  // namespace

void ReplacedBox::UpdateUsedSize(const LayoutParams& layout_params) {
  // TODO(***REMOVED***): See if we can determine and use the intrinsic element size.
  float intrinsic_width = kDefaultIntrinsicWidth;
  float intrinsic_height = kDefaultIntrinsicHeight;
  float intrinsic_ratio = kDefaultIntrinsicWidth / kDefaultIntrinsicHeight;

  ReplacedBoxUsedWidthProvider used_width_provider(
      layout_params.containing_block_size.width(), intrinsic_width);
  computed_style()->width()->Accept(&used_width_provider);

  ReplacedBoxUsedHeightProvider used_height_provider(
      layout_params.containing_block_size.height(), intrinsic_height);
  computed_style()->height()->Accept(&used_height_provider);

  float new_used_width = used_width_provider.used_width();
  float new_used_height = used_height_provider.used_height();

  if (used_width_provider.width_is_auto() &&
      !used_height_provider.height_is_auto()) {
    // If width is auto and height is not, then use the intrinsic ratio to
    // calculate the width, according to:
    // http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width
    new_used_width = used_height_provider.used_height() * intrinsic_ratio;
  } else if (used_height_provider.height_is_auto() &&
             !used_width_provider.width_is_auto()) {
    // If height is auto and width is not, then use the intrinsic ratio to
    // calculate the height, according to:
    // http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-height
    new_used_height = used_width_provider.used_width() / intrinsic_ratio;
  }

  // TODO(***REMOVED***): Implement margins, borders, and paddings.
  set_used_width(new_used_width);
  set_used_height(new_used_height);
}

scoped_ptr<Box> ReplacedBox::TrySplitAt(float /*available_width*/) {
  return scoped_ptr<Box>();
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
