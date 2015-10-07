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

#ifndef LAYOUT_TEXT_BOX_H_
#define LAYOUT_TEXT_BOX_H_

#include <string>

#include "cobalt/layout/box.h"
#include "cobalt/layout/paragraph.h"
#include "cobalt/render_tree/font.h"

namespace cobalt {
namespace layout {

// Although the CSS 2.1 specification assumes that the text is simply a part of
// an inline box, it is impractical to implement it that way. Instead, we define
// a text box as an anonymous replaced inline-level box that is breakable
// at soft wrap opportunities.
class TextBox : public Box {
 public:
  TextBox(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions,
      const UsedStyleProvider* used_style_provider,
      const scoped_refptr<Paragraph>& paragraph, int32 text_start_position,
      int32 text_end_position, bool triggers_line_break);

  // From |Box|.
  Level GetLevel() const OVERRIDE;

  void UpdateContentSizeAndMargins(const LayoutParams& layout_params) OVERRIDE;
  scoped_ptr<Box> TrySplitAt(float available_width,
                             bool allow_overflow) OVERRIDE;

  void SplitBidiLevelRuns() OVERRIDE;
  scoped_ptr<Box> TrySplitAtSecondBidiLevelRun() OVERRIDE;
  base::optional<int> GetBidiLevel() const OVERRIDE;

  void SetShouldCollapseLeadingWhiteSpace(
      bool should_collapse_leading_white_space) OVERRIDE;
  void SetShouldCollapseTrailingWhiteSpace(
      bool should_collapse_trailing_white_space) OVERRIDE;
  bool HasLeadingWhiteSpace() const OVERRIDE;
  bool HasTrailingWhiteSpace() const OVERRIDE;
  bool IsCollapsed() const OVERRIDE;

  bool JustifiesLineExistence() const OVERRIDE;
  bool DoesTriggerLineBreak() const OVERRIDE;
  bool AffectsBaselineInBlockFormattingContext() const OVERRIDE;
  float GetBaselineOffsetFromTopMarginEdge() const OVERRIDE;

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
  bool WhiteSpaceStyleAllowsCollapsing();
  bool WhiteSpaceStyleAllowsWrapping();

  void UpdateTextHasLeadingWhiteSpace();
  void UpdateTextHasTrailingWhiteSpace();

  // Split the text box into two.
  // |split_start_position| indicates the position within the paragraph where
  // the split occurs and the second box begins
  // |pre_split_width| indicates the width of the text that is remaining part
  // of the first box
  scoped_ptr<Box> SplitAtPosition(int32 split_start_position);

  // Width of a space character in the used font, if the box has leading white
  // space.
  float GetLeadingWhiteSpaceWidth() const;
  // Width of a space character in the used font, if the box has trailing white
  // space.
  float GetTrailingWhiteSpaceWidth() const;

  int32 GetNonCollapsibleTextStartPosition() const;
  int32 GetNonCollapsibleTextEndPosition() const;
  bool HasNonCollapsibleText() const;
  std::string GetNonCollapsibleText() const;

  // The paragraph that this text box is part of. It contains access to the
  // underlying text, and handles the logic for determining bidi levels and
  // where to split the text box during line breaking when the text box does
  // not fully fit on a line.
  const scoped_refptr<Paragraph> paragraph_;
  // The position within the paragraph where the text contained in this box
  // begins.
  int32 text_start_position_;
  // The position within the paragraph where the text contained in this box
  // ends.
  int32 text_end_position_;

  // A font used for text width and line height calculations.
  const scoped_refptr<render_tree::Font> used_font_;

  // Whitespace tracking.
  bool text_has_leading_white_space_;
  bool text_has_trailing_white_space_;
  bool should_collapse_leading_white_space_;
  bool should_collapse_trailing_white_space_;

  // Specifies whether this text box ends the line it is on, forcing any
  // additional sibling boxes to be added to a new line.
  bool triggers_line_break_;

  // A width of the space character in the used font, measured during layout.
  base::optional<float> space_width_;
  // A vertical offset of the baseline relatively to the origin of the text box.
  base::optional<float> baseline_offset_from_top_;

  bool update_size_results_valid_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_TEXT_BOX_H_
