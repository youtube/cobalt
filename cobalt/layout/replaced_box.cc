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

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/layout/container_box.h"
#include "cobalt/layout/letterboxed_image.h"
#include "cobalt/layout/used_style.h"
#include "cobalt/layout/white_space_processing.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/math/vector2d_f.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/rect_node.h"

namespace cobalt {
namespace layout {

using render_tree::animations::NodeAnimationsMap;
using render_tree::CompositionNode;
using render_tree::ImageNode;
using render_tree::RectNode;
using render_tree::SolidColorBrush;

namespace {

// Used when intrinsic ratio cannot be determined,
// as per https://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width.
const float kFallbackIntrinsicRatio = 2.0f;

// Becomes a used value of "width" if it cannot be determined by any other
// means, as per https://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width.
const float kFallbackWidth = 300.0f;

}  // namespace

ReplacedBox::ReplacedBox(
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        css_computed_style_declaration,
    const ReplaceImageCB& replace_image_cb,
    const scoped_refptr<Paragraph>& paragraph, int32 text_position,
    const base::optional<LayoutUnit>& maybe_intrinsic_width,
    const base::optional<LayoutUnit>& maybe_intrinsic_height,
    const base::optional<float>& maybe_intrinsic_ratio,
    UsedStyleProvider* used_style_provider)
    : Box(css_computed_style_declaration, used_style_provider),
      maybe_intrinsic_width_(maybe_intrinsic_width),
      maybe_intrinsic_height_(maybe_intrinsic_height),
      // Like Chromium, we assume that an element must always have an intrinsic
      // ratio, although technically it's a spec violation. For details see
      // https://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width.
      intrinsic_ratio_(maybe_intrinsic_ratio.value_or(kFallbackIntrinsicRatio)),
      replace_image_cb_(replace_image_cb),
      paragraph_(paragraph),
      text_position_(text_position) {
  DCHECK(!replace_image_cb_.is_null());
}

WrapResult ReplacedBox::TryWrapAt(
    WrapAtPolicy /*wrap_at_policy*/,
    WrapOpportunityPolicy wrap_opportunity_policy,
    bool is_line_existence_justified, LayoutUnit /*available_width*/,
    bool /*should_collapse_trailing_white_space*/) {
  // NOTE: This logic must stay in sync with
  // InlineLevelBlockContainerBox::TryWrapAt().

  // Wrapping is not allowed until the line's existence is justified, meaning
  // that wrapping cannot occur before the box. Given that this box cannot be
  // split, no wrappable point is available.
  if (!is_line_existence_justified) {
    return kWrapResultNoWrap;
  }

  // Atomic inline elements participate in the inline formatting context as a
  // single opaque box. Therefore, the parent's style should be used, as the
  // internals of the atomic inline element have no impact on the formatting of
  // the line.
  // https://www.w3.org/TR/CSS21/visuren.html#inline-boxes
  if (!parent()) {
    return kWrapResultNoWrap;
  }

  bool style_allows_break_word = parent()->computed_style()->overflow_wrap() ==
                                 cssom::KeywordValue::GetBreakWord();

  if (!ShouldProcessWrapOpportunityPolicy(wrap_opportunity_policy,
                                          style_allows_break_word)) {
    return kWrapResultNoWrap;
  }

  // Even when the style prevents wrapping, wrapping can still occur before the
  // box if the line's existence has already been justified and whitespace
  // precedes it.
  if (!DoesAllowTextWrapping(parent()->computed_style()->white_space())) {
    if (text_position_ > 0 &&
        paragraph_->IsCollapsibleWhiteSpace(text_position_ - 1)) {
      return kWrapResultWrapBefore;
    } else {
      return kWrapResultNoWrap;
    }
  }

  Paragraph::BreakPolicy break_policy =
      Paragraph::GetBreakPolicyFromWrapOpportunityPolicy(
          wrap_opportunity_policy, style_allows_break_word);
  return paragraph_->IsBreakPosition(text_position_, break_policy)
             ? kWrapResultWrapBefore
             : kWrapResultNoWrap;
}

void ReplacedBox::SplitBidiLevelRuns() {}

bool ReplacedBox::TrySplitAtSecondBidiLevelRun() { return false; }

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

bool ReplacedBox::AffectsBaselineInBlockFormattingContext() const {
  return false;
}

LayoutUnit ReplacedBox::GetBaselineOffsetFromTopMarginEdge() const {
  return GetMarginBoxHeight();
}

namespace {

void AddLetterboxedImageToRenderTree(
    const LetterboxDimensions& dimensions,
    const scoped_refptr<render_tree::Image>& image,
    CompositionNode::Builder* composition_node_builder) {
  const render_tree::ColorRGBA kSolidBlack(0, 0, 0, 1);

  if (dimensions.image_rect) {
    ImageNode::Builder image_builder(image, *dimensions.image_rect);
    image_builder.punch_through_alpha = true;
    composition_node_builder->AddChild(new ImageNode(image_builder));
  }

  for (uint32 i = 0; i < dimensions.fill_rects.size(); ++i) {
    const math::RectF& fill_rect = dimensions.fill_rects[i];
    composition_node_builder->AddChild(new RectNode(
        fill_rect,
        scoped_ptr<render_tree::Brush>(new SolidColorBrush(kSolidBlack))));
  }
}

void AnimateCB(ReplacedBox::ReplaceImageCB replace_image_cb,
               math::SizeF destination_size,
               CompositionNode::Builder* composition_node_builder,
               base::TimeDelta time) {
  UNREFERENCED_PARAMETER(time);

  DCHECK(!replace_image_cb.is_null());
  DCHECK(composition_node_builder);

  scoped_refptr<render_tree::Image> image = replace_image_cb.Run();

  // TODO(***REMOVED***): Detect better when the intrinsic video size is used for the
  //   node size, and trigger a re-layout from the media element when the size
  //   changes.
  if (image && 0 == destination_size.height()) {
    destination_size = image->GetSize();
  }

  AddLetterboxedImageToRenderTree(
      GetLetterboxDimensions(image->GetSize(), destination_size), image,
      composition_node_builder);
}

}  // namespace
void ReplacedBox::RenderAndAnimateContent(
    CompositionNode::Builder* border_node_builder,
    NodeAnimationsMap::Builder* node_animations_map_builder) const {
  if (computed_style()->visibility() != cssom::KeywordValue::GetVisible()) {
    return;
  }

  CompositionNode::Builder composition_node_builder(
      math::Vector2dF((border_left_width() + padding_left()).toFloat(),
                      (border_top_width() + padding_top()).toFloat()));

  scoped_refptr<CompositionNode> composition_node =
      new CompositionNode(composition_node_builder);
  node_animations_map_builder->Add(
      composition_node,
      base::Bind(AnimateCB, replace_image_cb_, content_box_size()));
  border_node_builder->AddChild(composition_node);
}

void ReplacedBox::UpdateContentSizeAndMargins(
    const LayoutParams& layout_params) {
  base::optional<LayoutUnit> maybe_width = GetUsedWidthIfNotAuto(
      computed_style(), layout_params.containing_block_size, NULL);
  base::optional<LayoutUnit> maybe_height = GetUsedHeightIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::optional<LayoutUnit> maybe_left = GetUsedLeftIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::optional<LayoutUnit> maybe_top = GetUsedTopIfNotAuto(
      computed_style(), layout_params.containing_block_size);

  if (IsAbsolutelyPositioned()) {
    // TODO(***REMOVED***): Implement CSS section 10.3.8, see
    // https://www.w3.org/TR/CSS21/visudet.html#abs-replaced-width.
    set_left(maybe_left.value_or(LayoutUnit()));
    set_top(maybe_top.value_or(LayoutUnit()));
  }
  if (!maybe_width) {
    if (!maybe_height) {
      if (maybe_intrinsic_width_) {
        // If "height" and "width" both have computed values of "auto" and
        // the element also has an intrinsic width, then that intrinsic width
        // is the used value of "width".
        //   https://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width
        set_width(*maybe_intrinsic_width_);
      } else if (maybe_intrinsic_height_) {
        // If "height" and "width" both have computed values of "auto" and
        // the element has no intrinsic width, but does have an intrinsic height
        // and intrinsic ratio then the used value of "width" is:
        //     (intrinsic height) * (intrinsic ratio)
        //   https://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width
        set_width(*maybe_intrinsic_height_ * intrinsic_ratio_);
      } else {
        // Otherwise, if "width" has a computed value of "auto", but none of
        // the conditions above are met, then the used value of "width" becomes
        // 300px.
        //   https://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width
        set_width(LayoutUnit(kFallbackWidth));
      }
    } else {
      // If "width" has a computed value of "auto", "height" has some other
      // computed value, and the element does have an intrinsic ratio then
      // the used value of "width" is:
      //     (used height) * (intrinsic ratio)
      //   https://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width
      set_width(*maybe_height * intrinsic_ratio_);
    }
  } else {
    set_width(*maybe_width);
  }

  if (!maybe_height) {
    if (!maybe_width && maybe_intrinsic_height_) {
      // If "height" and "width" both have computed values of "auto" and
      // the element also has an intrinsic height, then that intrinsic height
      // is the used value of "height".
      //   https://www.w3.org/TR/CSS21/visudet.html#inline-replaced-height
      set_height(*maybe_intrinsic_height_);
    } else {
      // Otherwise, if "height" has a computed value of "auto", and the element
      // has an intrinsic ratio then the used value of "height" is:
      //     (used width) / (intrinsic ratio)
      //   https://www.w3.org/TR/CSS21/visudet.html#inline-replaced-height
      set_height(width() / intrinsic_ratio_);
    }
  } else {
    set_height(*maybe_height);
  }

  if (!maybe_width && !maybe_height) {
    // For replaced elements with an intrinsic ratio and both 'width' and
    // 'height' specified as 'auto', the algorithm is as described in
    // https://www.w3.org/TR/CSS21/visudet.html#min-max-widths.

    base::optional<LayoutUnit> maybe_max_width = GetUsedMaxWidthIfNotNone(
        computed_style(), layout_params.containing_block_size, NULL);
    LayoutUnit min_width = GetUsedMinWidth(
        computed_style(), layout_params.containing_block_size, NULL);
    base::optional<LayoutUnit> maybe_max_height = GetUsedMaxHeightIfNotNone(
        computed_style(), layout_params.containing_block_size, NULL);
    LayoutUnit min_height = GetUsedMinHeight(
        computed_style(), layout_params.containing_block_size, NULL);

    // The values w and h stand for the results of the width and height
    // computations ignoring the 'min-width', 'min-height', 'max-width' and
    // 'max-height' properties. Normally these are the intrinsic width and
    // height, but they may not be in the case of replaced elements with
    // intrinsic ratios.
    //   https://www.w3.org/TR/CSS21/visudet.html#min-max-widths
    LayoutUnit w = width();
    LayoutUnit h = height();

    // Take the max-width and max-height as max(min, max) so that min <= max
    // holds true.
    //   https://www.w3.org/TR/CSS21/visudet.html#min-max-widths
    base::optional<LayoutUnit> max_height;
    bool h_greater_than_max_height = false;
    if (maybe_max_height) {
      max_height = std::max(min_height, *maybe_max_height);
      h_greater_than_max_height = h > *max_height;
    }

    base::optional<LayoutUnit> max_width;
    bool w_greater_than_max_width = false;
    if (maybe_max_width) {
      max_width = std::max(min_width, *maybe_max_width);
      w_greater_than_max_width = w > *max_width;
    }

    // This block sets resolved width and resolved height values according to
    // the table listing a number of different constraint violations in
    // https://www.w3.org/TR/CSS21/visudet.html#min-max-widths.
    if (w_greater_than_max_width) {
      if (h_greater_than_max_height) {
        LayoutUnit max_width_ratio = *max_width / w.toFloat();
        LayoutUnit max_height_ratio = *max_height / h.toFloat();
        if (max_width_ratio > max_height_ratio) {
          // Constraint: (w > max-width) and (h > max-height), where
          // (max-width/w > max-height/h)
          set_width(
              std::max(min_width, *max_height * (w.toFloat() / h.toFloat())));
          set_height(*max_height);
        } else {
          // Constraint: (w > max-width) and (h > max-height), where
          // (max-width/w <= max-height/h)
          set_width(*max_width);
          set_height(
              std::max(min_height, *max_width * (h.toFloat() / w.toFloat())));
        }
      } else {  // not h_greater_than_max_height
        if (h < min_height) {
          // Constraint: (w > max-width) and (h < min-height)
          set_width(*max_width);
          set_height(min_height);
        } else {  // not h < min_height
          // Constraint: w > max-width
          set_width(*max_width);
          set_height(
              std::max(*max_width * (h.toFloat() / w.toFloat()), min_height));
        }
      }
    } else {  // not w_greater_than_max_width
      if (w < min_width) {
        if (h_greater_than_max_height) {
          // Constraint: (w < min-width) and (h > max-height)
          set_width(min_width);
          set_height(*max_height);
        } else {  // not h_greater_than_max_height
          if (h < min_height) {
            LayoutUnit min_width_ratio = min_width / w.toFloat();
            LayoutUnit min_height_ratio = min_height / h.toFloat();
            if (min_width_ratio > min_height_ratio) {
              // Constraint: (w < min-width) and (h < min-height), where
              // (min-width/w > min-height/h)
              set_width(min_width);
              set_height(std::min(*max_height,
                                  min_width * (h.toFloat() / w.toFloat())));
            } else {
              // Constraint: (w < min-width) and (h < min-height), where
              // (min-width/w <= min-height/h)
              set_width(std::min(*max_width,
                                 min_height * (w.toFloat() / h.toFloat())));
              set_height(min_height);
            }
          } else {  // not h < min-height
            // Constraint: w < min-width
            set_width(min_width);
            set_height(
                std::min(min_width * (h.toFloat() / w.toFloat()), *max_height));
          }
        }
      } else {  // not w < min_width
        if (h_greater_than_max_height) {
          // Constraint: h > max-height
          set_width(
              std::max(*max_height * (w.toFloat() / h.toFloat()), min_width));
          set_height(*max_height);
        } else {  // not h_greater_than_max_height
          if (h < min_height) {
            // Constraint: h < min-height
            set_width(
                std::min(min_height * (w.toFloat() / h.toFloat()), *max_width));
            set_height(min_height);
          } else {  // not h < min_height
            // Constraint: none
            // Do nothing (keep w and h).
          }
        }
      }
    }
  }

  // The horizontal margin rules are difference for block level replaced boxes
  // versus inline level replaced boxes.
  //   https://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width
  //   https://www.w3.org/TR/CSS21/visudet.html#block-replaced-width
  base::optional<LayoutUnit> maybe_margin_left = GetUsedMarginLeftIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::optional<LayoutUnit> maybe_margin_right = GetUsedMarginRightIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  LayoutUnit border_box_width = GetBorderBoxWidth();
  UpdateHorizontalMargins(layout_params.containing_block_size.width(),
                          border_box_width, maybe_margin_left,
                          maybe_margin_right);

  base::optional<LayoutUnit> maybe_margin_top = GetUsedMarginTopIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::optional<LayoutUnit> maybe_margin_bottom = GetUsedMarginBottomIfNotAuto(
      computed_style(), layout_params.containing_block_size);

  // If "margin-top", or "margin-bottom" are "auto", their used value is 0.
  //   https://www.w3.org/TR/CSS21/visudet.html#inline-replaced-height
  set_margin_top(maybe_margin_top.value_or(LayoutUnit()));
  set_margin_bottom(maybe_margin_bottom.value_or(LayoutUnit()));
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
