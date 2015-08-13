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

#include "cobalt/layout/box.h"
#include "cobalt/layout/paragraph.h"
#include "cobalt/render_tree/font.h"

namespace cobalt {
namespace layout {

// Although the CSS 2.1 specification assumes that the text is simply a part of
// an inline box, it is impractical to implement it that way. Instead, we define
// a text box as an anonymous non-atomic inline-level box. During the layout
// a text inside the text box is lazily divided into segments between break
// points (soft wrap opportunities), and those segments act as inline-level
// replaced boxes.
class TextBox : public Box {
 public:
  static const float kInvalidTextWidth;

  TextBox(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions,
      const UsedStyleProvider* used_style_provider,
      const scoped_refptr<Paragraph>& paragraph, int32 paragraph_start,
      int32 paragraph_end, float text_width);

  // From |Box|.
  Level GetLevel() const OVERRIDE;

  void UpdateUsedSize(const LayoutParams& layout_params) OVERRIDE;
  scoped_ptr<Box> TrySplitAt(float available_width,
                             bool allow_overflow) OVERRIDE;

  void SplitBidiLevelRuns() OVERRIDE;
  scoped_ptr<Box> TrySplitAtSecondBidiLevelRun() OVERRIDE;
  base::optional<int> GetBidiLevel() const OVERRIDE;

  bool IsCollapsed() const OVERRIDE;
  bool HasLeadingWhiteSpace() const OVERRIDE;
  bool HasTrailingWhiteSpace() const OVERRIDE;
  void CollapseLeadingWhiteSpace() OVERRIDE;
  void CollapseTrailingWhiteSpace() OVERRIDE;

  bool JustifiesLineExistence() const OVERRIDE;
  bool AffectsBaselineInBlockFormattingContext() const OVERRIDE;
  float GetHeightAboveBaseline() const OVERRIDE;

 protected:
  // From |Box|.
  void AddContentToRenderTree(
      render_tree::CompositionNode::Builder* composition_node_builder,
      render_tree::animations::NodeAnimationsMap::Builder*
          node_animations_map_builder) const OVERRIDE;
  bool IsTransformable() const OVERRIDE;

  void DumpClassName(std::ostream* stream) const OVERRIDE;
  void DumpProperties(std::ostream* stream) const OVERRIDE;
  void DumpChildrenWithIndent(std::ostream* stream, int indent) const OVERRIDE;

 private:
  void UpdateFontData();
  void UpdateTextData();
  void UpdateParagraphStartPositionWhiteSpace();
  void UpdateParagraphEndPositionWhiteSpace();
  void UpdateTextWidthIfInvalid();

  std::string GetText() const;

  // Split the text box into two.
  // |split_start_position| indicates the position within the paragraph where
  // the split occurs and the second box begins
  // |pre_split_width| indicates the width of the text that is remaining part
  // of the first box
  scoped_ptr<Box> SplitAtPosition(int32 split_start_position,
                                  float pre_split_width);

  // Width of a space character in the used font, if the box has leading white
  // space.
  float GetLeadingWhiteSpaceWidth() const;
  // Width of a space character in the used font, if the box has trailing white
  // space.
  float GetTrailingWhiteSpaceWidth() const;

  bool HasText() const;
  int32 GetTextStartPosition() const;
  int32 GetTextEndPosition() const;

  bool IsTextWidthInvalid() const;

  // The paragraph that this text box is part of. It contains access to the
  // underlying text, and handles the logic for determining bidi levels and
  // where to split the text box during line breaking when the text box does
  // not fully fit on a line.
  const scoped_refptr<Paragraph> paragraph_;
  // The position within the paragraph where the text contained in this box
  // begins.
  int32 paragraph_start_position_;
  // The position within the paragraph where the text contained in this box
  // ends.
  int32 paragraph_end_position_;
  // The total width of the text in the box, based on the font.
  float text_width_;

  // Whitespace tracking
  bool is_paragraph_start_position_white_space_;
  bool is_paragraph_end_position_white_space_;
  bool is_trailing_white_space_collapsed_;
  bool is_leading_white_space_collapsed_;

  // A font used for text width and line height calculations.
  const scoped_refptr<render_tree::Font> used_font_;
  // A vertical offset of the baseline relatively to the origin of the text box.
  float height_above_baseline_;
  // The height of the inline box encloses all glyphs and their half-leading on
  // each side
  float used_line_height_;
  // A width of the space character in the used font, measured during layout.
  float space_width_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_TEXT_BOX_H_
