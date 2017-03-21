// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_LAYOUT_REPLACED_BOX_H_
#define COBALT_LAYOUT_REPLACED_BOX_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/paragraph.h"
#include "cobalt/math/rect.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/punch_through_video_node.h"

namespace cobalt {
namespace layout {

// The class represents a Replaced element in the layout tree. It is used to
// render elements like embed, iframe or video. Currently it renders the element
// as an image retrieved from a callback passed into its ctor.
//   https://www.w3.org/TR/html5/rendering.html#replaced-elements
//
// TODO: Make ReplacedBox support elements other than media element.
class ReplacedBox : public Box {
 public:
  typedef base::Callback<scoped_refptr<render_tree::Image>()> ReplaceImageCB;
  typedef render_tree::PunchThroughVideoNode::SetBoundsCB SetBoundsCB;

  ReplacedBox(const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
                  css_computed_style_declaration,
              const ReplaceImageCB& replace_image_cb,
              const SetBoundsCB& set_bounds_cb,
              const scoped_refptr<Paragraph>& paragraph, int32 text_position,
              const base::optional<LayoutUnit>& maybe_intrinsic_width,
              const base::optional<LayoutUnit>& maybe_intrinsic_height,
              const base::optional<float>& maybe_intrinsic_ratio,
              UsedStyleProvider* used_style_provider,
              base::optional<bool> is_video_punched_out,
              LayoutStatTracker* layout_stat_tracker);

  // From |Box|.
  WrapResult TryWrapAt(WrapAtPolicy wrap_at_policy,
                       WrapOpportunityPolicy wrap_opportunity_policy,
                       bool is_line_existence_justified,
                       LayoutUnit available_width,
                       bool should_collapse_trailing_white_space) OVERRIDE;

  void SplitBidiLevelRuns() OVERRIDE;
  bool TrySplitAtSecondBidiLevelRun() OVERRIDE;
  base::optional<int> GetBidiLevel() const OVERRIDE;

  void SetShouldCollapseLeadingWhiteSpace(
      bool should_collapse_leading_white_space) OVERRIDE;
  void SetShouldCollapseTrailingWhiteSpace(
      bool should_collapse_trailing_white_space) OVERRIDE;
  bool HasLeadingWhiteSpace() const OVERRIDE;
  bool HasTrailingWhiteSpace() const OVERRIDE;
  bool IsCollapsed() const OVERRIDE;

  bool JustifiesLineExistence() const OVERRIDE;
  bool AffectsBaselineInBlockFormattingContext() const OVERRIDE;
  LayoutUnit GetBaselineOffsetFromTopMarginEdge() const OVERRIDE;

 protected:
  // From |Box|.
  void UpdateContentSizeAndMargins(const LayoutParams& layout_params) OVERRIDE;

  void RenderAndAnimateContent(render_tree::CompositionNode::Builder*
                                   border_node_builder) const OVERRIDE;

  bool IsTransformable() const OVERRIDE { return true; }

#ifdef COBALT_BOX_DUMP_ENABLED
  void DumpProperties(std::ostream* stream) const OVERRIDE;
#endif  // COBALT_BOX_DUMP_ENABLED

  // Rest of the protected methods.

  // Updates used values of "margin-left" and "margin-right" properties based on
  // https://www.w3.org/TR/CSS21/visudet.html#block-replaced-width and
  // https://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width.
  virtual void UpdateHorizontalMargins(
      LayoutUnit containing_block_width, LayoutUnit border_box_width,
      const base::optional<LayoutUnit>& maybe_margin_left,
      const base::optional<LayoutUnit>& maybe_margin_right) = 0;

  // TODO: Make private.
  const base::optional<LayoutUnit> maybe_intrinsic_width_;
  const base::optional<LayoutUnit> maybe_intrinsic_height_;
  const float intrinsic_ratio_;

 private:
  const ReplaceImageCB replace_image_cb_;
  const SetBoundsCB set_bounds_cb_;

  const scoped_refptr<Paragraph> paragraph_;
  int32 text_position_;
  base::optional<bool> is_video_punched_out_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_REPLACED_BOX_H_
