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

#ifndef COBALT_LAYOUT_TEXT_BOX_H_
#define COBALT_LAYOUT_TEXT_BOX_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "cobalt/dom/font_list.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/layout_unit.h"
#include "cobalt/layout/paragraph.h"
#include "cobalt/render_tree/font.h"

namespace cobalt {
namespace layout {

// Although the CSS 2.1 specification assumes that the text is simply a part of
// an inline box, it is impractical to implement it that way. Instead, we define
// a text box as an anonymous replaced inline-level box that is breakable at
// soft wrap opportunities.
class TextBox : public Box {
 public:
  TextBox(const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
              css_computed_style_declaration,
          const scoped_refptr<Paragraph>& paragraph, int32 text_start_position,
          int32 text_end_position, bool triggers_line_break,
          UsedStyleProvider* used_style_provider);

  // From |Box|.
  Level GetLevel() const OVERRIDE;

  void UpdateContentSizeAndMargins(const LayoutParams& layout_params) OVERRIDE;

  WrapResult TryWrapAt(WrapAtPolicy wrap_at_policy,
                       WrapOpportunityPolicy wrap_opportunity_policy,
                       bool is_line_existence_justified,
                       LayoutUnit available_width,
                       bool should_collapse_trailing_white_space) OVERRIDE;

  Box* GetSplitSibling() const OVERRIDE;

  bool DoesFulfillEllipsisPlacementRequirement() const OVERRIDE;
  void ResetEllipses() OVERRIDE;

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
  bool HasTrailingLineBreak() const OVERRIDE;
  bool AffectsBaselineInBlockFormattingContext() const OVERRIDE;
  LayoutUnit GetBaselineOffsetFromTopMarginEdge() const OVERRIDE;
  LayoutUnit GetInlineLevelBoxHeight() const OVERRIDE;
  LayoutUnit GetInlineLevelTopMargin() const OVERRIDE;

  bool ValidateUpdateSizeInputs(const LayoutParams& params) OVERRIDE;

 protected:
  // From |Box|.
  void RenderAndAnimateContent(
      render_tree::CompositionNode::Builder* border_node_builder,
      render_tree::animations::NodeAnimationsMap::Builder*
          node_animations_map_builder) const OVERRIDE;
  bool IsTransformable() const OVERRIDE;

#ifdef COBALT_BOX_DUMP_ENABLED
  void DumpClassName(std::ostream* stream) const OVERRIDE;
  void DumpProperties(std::ostream* stream) const OVERRIDE;
  void DumpChildrenWithIndent(std::ostream* stream, int indent) const OVERRIDE;
#endif  // COBALT_BOX_DUMP_ENABLED

 private:
  struct CachedGlyphBufferInfo {
    CachedGlyphBufferInfo() : start_position(-1), length(0) {}

    scoped_refptr<render_tree::GlyphBuffer> glyph_buffer;
    int32 start_position;
    int32 length;
  };

  // From |Box|.
  void DoPlaceEllipsisOrProcessPlacedEllipsis(
      BaseDirection base_direction, LayoutUnit desired_offset,
      bool* is_placement_requirement_met, bool* is_placed,
      LayoutUnit* placed_offset) OVERRIDE;

  void UpdateTextHasLeadingWhiteSpace();
  void UpdateTextHasTrailingWhiteSpace();

  int32 GetWrapPosition(WrapAtPolicy wrap_at_policy,
                        WrapOpportunityPolicy wrap_opportunity_policy,
                        bool is_line_existence_justified,
                        LayoutUnit available_width,
                        bool should_collapse_trailing_white_space,
                        bool style_allows_break_word, int32 start_position);

  // Split the text box into two.
  // |split_start_position| indicates the position within the paragraph where
  // the split occurs and the second box begins
  void SplitAtPosition(int32 split_start_position);

  // Width of a space character in the used font, if the box has leading white
  // space.
  LayoutUnit GetLeadingWhiteSpaceWidth() const;
  // Width of a space character in the used font, if the box has trailing white
  // space.
  LayoutUnit GetTrailingWhiteSpaceWidth() const;

  int32 GetNonCollapsedTextStartPosition() const;
  int32 GetNonCollapsedTextEndPosition() const;

  int32 GetNonCollapsibleTextStartPosition() const;
  int32 GetNonCollapsibleTextEndPosition() const;
  int32 GetNonCollapsibleTextLength() const;
  bool HasNonCollapsibleText() const;
  std::string GetNonCollapsibleText() const;

  int32 GetVisibleTextEndPosition() const;
  int32 GetVisibleTextLength() const;
  bool HasVisibleText() const;
  std::string GetVisibleText() const;

  // The paragraph that this text box is part of. It contains access to the
  // underlying text, and handles the logic for determining bidi levels and
  // where to split the text box during line breaking when the text box does
  // not fully fit on a line.
  const scoped_refptr<Paragraph> paragraph_;
  // The position within the paragraph where the text contained in this box
  // begins.
  const int32 text_start_position_;
  // The position within the paragraph where the text contained in this box
  // ends.
  int32 text_end_position_;
  // The position within the paragraph where the text within the text box is
  // truncated by an ellipsis and will not be visible.
  // "Implementations must hide characters and atomic inline-level elements at
  // the applicable edge(s) of the line as necessary to fit the ellipsis."
  //   https://www.w3.org/TR/css3-ui/#propdef-text-overflow
  int32 truncated_text_end_position_;
  // The horizontal offset to apply to rendered text as a result of an ellipsis
  // truncating the text. This value can be non-zero when the text box is in a
  // line with a right-to-left base direction. In this case, when an ellipsis is
  // placed, the truncated text is offset to begin to the right of the ellipsis.
  float truncated_text_offset_from_left_;

  // A font used for text width and line height calculations.
  const scoped_refptr<dom::FontList> used_font_;

  // Whitespace tracking.
  bool text_has_leading_white_space_;
  bool text_has_trailing_white_space_;
  bool should_collapse_leading_white_space_;
  bool should_collapse_trailing_white_space_;

  // Specifies whether this text box ends the line it is on, forcing any
  // additional sibling boxes to be added to a new line.
  bool has_trailing_line_break_;

  // A vertical offset of the baseline relatively to the origin of the text box.
  base::optional<LayoutUnit> baseline_offset_from_top_;

  // A reference to the next text box in a linked list of text boxes produced
  // from splits of the initial text box. This enables HTMLElement to retain
  // access to all of its layout boxes after they are split.
  scoped_refptr<TextBox> split_sibling_;

  bool update_size_results_valid_;
  LayoutUnit line_height_;
  LayoutUnit inline_top_margin_;
  float ascent_;

  // The width of the portion of the text that is unaffected by whitespace
  // collapsing.
  base::optional<LayoutUnit> non_collapsible_text_width_;

  // Glyph buffer caching is used to prevent it from needing to be recalculated
  // during each call to RenderAndAnimateContent. It is mutable as a result of
  // RenderAndAnimateContent being const.
  mutable CachedGlyphBufferInfo cached_glyph_buffer_info_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_TEXT_BOX_H_
