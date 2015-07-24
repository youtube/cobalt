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
  TextBox(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions, const std::string& text,
      bool has_leading_white_space, bool has_trailing_white_space,
      const UsedStyleProvider* used_style_provider);

  // From |Box|.
  Level GetLevel() const OVERRIDE;

  void UpdateUsedSize(const LayoutParams& layout_params) OVERRIDE;
  scoped_ptr<Box> TrySplitAt(float available_width,
                             bool allow_overflow) OVERRIDE;

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
  // Width of a space character in the used font, if the box has leading white
  // space.
  float GetLeadingWhiteSpaceWidth() const;
  // Width of a space character in the used font, if the box has trailing white
  // space. If the box has no text, returns 0 (because the leading white space
  // has already contributed to the width).
  float GetTrailingWhiteSpaceWidth() const;

  // A processed text in which:
  //   - collapsible white space has been collapsed and trimmed for both ends;
  //   - segment breaks have been transformed;
  //   - letter case has been transformed.
  std::string text_;
  // Whether the box has a collapsible leading white space. Note that actual
  // white space characters has been removed from the beginning of |text_|.
  // If the text is empty, |has_leading_white_space_| must be equal to
  // |has_trailing_white_space_|.
  bool has_leading_white_space_;
  // Whether the box has a collapsible trailing white space. Note that actual
  // white space characters has been removed from the end of |text_|.
  // If the text is empty, |has_leading_white_space_| must be equal to
  // |has_trailing_white_space_|.
  bool has_trailing_white_space_;
  // A font used for text width and line height calculations.
  const scoped_refptr<render_tree::Font> used_font_;
  // A vertical offset of the baseline relatively to the origin of the text box.
  float height_above_baseline_;
  // A width of a space character in the used font, measured during layout.
  float space_width_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_TEXT_BOX_H_
