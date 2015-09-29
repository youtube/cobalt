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
#include "cobalt/layout/create_letterboxed_image.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/color_rgba.h"

namespace cobalt {
namespace layout {

using render_tree::animations::NodeAnimationsMap;
using render_tree::CompositionNode;

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

void ReplacedBox::SetShouldCollapseLeadingWhiteSpace(
    bool /*should_collapse_leading_white_space*/) {
  // Do nothing.
}

void ReplacedBox::SetShouldCollapseTrailingWhiteSpace(
    bool /*should_collapse_trailing_white_space*/) {
  // Do nothing.
}

bool ReplacedBox::HasLeadingWhiteSpace() const { return false; }

bool ReplacedBox::HasTrailingWhiteSpace() const { return false; }

bool ReplacedBox::IsCollapsed() const { return false; }

bool ReplacedBox::JustifiesLineExistence() const { return true; }

bool ReplacedBox::DoesTriggerLineBreak() const { return false; }

bool ReplacedBox::AffectsBaselineInBlockFormattingContext() const {
  return false;
}

float ReplacedBox::GetBaselineOffsetFromTopMarginEdge() const {
  return GetMarginBoxHeight();
}

namespace {

void AnimateCB(ReplacedBox::ReplaceImageCB replace_image_cb,
               math::SizeF destination_size,
               CompositionNode::Builder* composition_node_builder,
               base::TimeDelta time) {
  UNREFERENCED_PARAMETER(time);

  const render_tree::ColorRGBA kSolidBlack(0, 0, 0, 1);

  DCHECK(!replace_image_cb.is_null());
  DCHECK(composition_node_builder);

  scoped_refptr<render_tree::Image> image = replace_image_cb.Run();

  // TODO(***REMOVED***): Detect better when the intrinsic video size is used for the
  //   node size, and trigger a re-layout from the media element when the size
  //   changes.
  if (image && 0 == destination_size.height()) {
    destination_size = image->GetSize();
  }

  CreateLetterboxedImage(image, destination_size, kSolidBlack,
                         composition_node_builder);
}

}  // namespace
void ReplacedBox::RenderAndAnimateContent(
    CompositionNode::Builder* border_node_builder,
    NodeAnimationsMap::Builder* node_animations_map_builder) const {
  CompositionNode::Builder composition_node_builder;

  scoped_refptr<CompositionNode> composition_node =
      new CompositionNode(composition_node_builder);
  node_animations_map_builder->Add(
      composition_node,
      base::Bind(AnimateCB, replace_image_cb_, content_box_size()));
  border_node_builder->AddChild(
      composition_node,
      math::TranslateMatrix(border_left_width() + padding_left(),
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
