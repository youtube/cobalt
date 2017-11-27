// Copyright 2014 Google Inc. All Rights Reserved.
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
          bool is_created_from_split, UsedStyleProvider* used_style_provider,
          LayoutStatTracker* layout_stat_tracker);

  // From |Box|.
  Level GetLevel() const override;
  TextBox* AsTextBox() override;
  const TextBox* AsTextBox() const override;

  void UpdateContentSizeAndMargins(const LayoutParams& layout_params) override;

  WrapResult TryWrapAt(WrapAtPolicy wrap_at_policy,
                       WrapOpportunityPolicy wrap_opportunity_policy,
                       bool is_line_existence_justified,
                       LayoutUnit available_width,
                       bool should_collapse_trailing_white_space) override;

  Box* GetSplitSibling() const override;

  bool DoesFulfillEllipsisPlacementRequirement() const override;
  void DoPreEllipsisPlacementProcessing() override;
  void DoPostEllipsisPlacementProcessing() override;

  void SplitBidiLevelRuns() override;
  bool TrySplitAtSecondBidiLevelRun() override;
  base::optional<int> GetBidiLevel() const override;

  void SetShouldCollapseLeadingWhiteSpace(
      bool should_collapse_leading_white_space) override;
  void SetShouldCollapseTrailingWhiteSpace(
      bool should_collapse_trailing_white_space) override;
  bool HasLeadingWhiteSpace() const override;
  bool HasTrailingWhiteSpace() const override;
  bool IsCollapsed() const override;

  bool JustifiesLineExistence() const override;
  bool HasTrailingLineBreak() const override;
  bool AffectsBaselineInBlockFormattingContext() const override;
  LayoutUnit GetBaselineOffsetFromTopMarginEdge() const override;
  LayoutUnit GetInlineLevelBoxHeight() const override;
  LayoutUnit GetInlineLevelTopMargin() const override;

  bool ValidateUpdateSizeInputs(const LayoutParams& params) override;

 protected:
  // From |Box|.
  void RenderAndAnimateContent(
      render_tree::CompositionNode::Builder* border_node_builder,
      ContainerBox* stacking_context) const override;
  bool IsTransformable() const override;

#ifdef COBALT_BOX_DUMP_ENABLED
  void DumpClassName(std::ostream* stream) const override;
  void DumpProperties(std::ostream* stream) const override;
  void DumpChildrenWithIndent(std::ostream* stream, int indent) const override;
#endif  // COBALT_BOX_DUMP_ENABLED

 private:
  // From |Box|.
  void DoPlaceEllipsisOrProcessPlacedEllipsis(
      BaseDirection base_direction, LayoutUnit desired_offset,
      bool* is_placement_requirement_met, bool* is_placed,
      LayoutUnit* placed_offset) override;

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
  // Tracking of the previous value of |truncated_text_end_position_|, which
  // allows for determination of whether or not the value changed during
  // ellipsis placement. When this occurs, the cached render tree nodes of this
  // box and its ancestors are invalidated.
  int32 previous_truncated_text_end_position_;
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

  // Specifies whether or not this text box was created as a result of the split
  // of a text box.
  const bool is_product_of_split_;

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
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_TEXT_BOX_H_
