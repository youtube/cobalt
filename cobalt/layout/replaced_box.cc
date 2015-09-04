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
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/image_node.h"

namespace cobalt {
namespace layout {

using render_tree::ImageNode;

namespace {

// Used when intrinsic ratio cannot be determined,
// as per http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width.
const float kFallbackIntrinsicRatio = 2.0f;

}  // namespace

ReplacedBox::ReplacedBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions,
    const UsedStyleProvider* used_style_provider,
    const ReplaceImageCB& replace_image_cb,
    const scoped_refptr<Paragraph>& paragraph, int32 text_position,
    const base::optional<float>& maybe_intrinsic_width,
    const base::optional<float>& maybe_intrinsic_height,
    const base::optional<float>& maybe_intrinsic_ratio)
    : Box(computed_style, transitions, used_style_provider),
      maybe_intrinsic_width_(maybe_intrinsic_width),
      maybe_intrinsic_height_(maybe_intrinsic_height),
      // Like Chromium, we assume that an element must always have an intrinsic
      // ratio, although technically it's a spec violation. For details see
      // http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width.
      intrinsic_ratio_(
          maybe_intrinsic_ratio.value_or(kFallbackIntrinsicRatio)),
      replace_image_cb_(replace_image_cb),
      paragraph_(paragraph),
      text_position_(text_position) {
  DCHECK(!replace_image_cb_.is_null());
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

bool ReplacedBox::IsCollapsed() const { return false; }

bool ReplacedBox::HasLeadingWhiteSpace() const { return false; }

bool ReplacedBox::HasTrailingWhiteSpace() const { return false; }

void ReplacedBox::CollapseLeadingWhiteSpace() {
  // Do nothing.
}

void ReplacedBox::CollapseTrailingWhiteSpace() {
  // Do nothing.
}

bool ReplacedBox::JustifiesLineExistence() const { return true; }

bool ReplacedBox::AffectsBaselineInBlockFormattingContext() const {
  return false;
}

float ReplacedBox::GetBaselineOffsetFromTopMarginEdge() const {
  return GetMarginBoxHeight();
}

namespace {

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

void ReplacedBox::RenderAndAnimateContent(
    render_tree::CompositionNode::Builder* border_node_builder,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animations_map_builder) const {
  scoped_refptr<ImageNode> image_node =
      new render_tree::ImageNode(NULL, content_box_size());
  node_animations_map_builder->Add(image_node,
                                   base::Bind(AnimateCB, replace_image_cb_));
  border_node_builder->AddChild(
      image_node, math::TranslateMatrix(border_left_width() + padding_left(),
                                        border_top_width() + padding_top()));
}

#ifdef COBALT_BOX_DUMP_ENABLED

void ReplacedBox::DumpProperties(std::ostream* stream) const {
  Box::DumpProperties(stream);

  *stream << "text_position=" << text_position_ << " "
          << "bidi_level=" << paragraph_->GetBidiLevel(text_position_) << " ";
}

#endif  // COBALT_BOX_DUMP_ENABLED

}  // namespace layout
}  // namespace cobalt
