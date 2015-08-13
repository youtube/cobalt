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

#include "cobalt/layout/text_box.h"

#include "cobalt/layout/used_style.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/text_node.h"

namespace cobalt {
namespace layout {

// static
const float TextBox::kInvalidTextWidth =
    std::numeric_limits<float>::quiet_NaN();

TextBox::TextBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions,
    const UsedStyleProvider* used_style_provider,
    const scoped_refptr<Paragraph>& paragraph, int32 paragraph_start_position,
    int32 paragraph_end_position, float text_width)
    : Box(computed_style, transitions, used_style_provider),
      paragraph_(paragraph),
      paragraph_start_position_(paragraph_start_position),
      paragraph_end_position_(paragraph_end_position),
      text_width_(text_width),
      is_paragraph_start_position_white_space_(false),
      is_paragraph_end_position_white_space_(false),
      is_trailing_white_space_collapsed_(false),
      is_leading_white_space_collapsed_(false),
      used_font_(used_style_provider->GetUsedFont(
          computed_style->font_family(), computed_style->font_size(),
          computed_style->font_style(), computed_style->font_weight())) {
  DCHECK(paragraph_start_position_ <= paragraph_end_position_);

  UpdateFontData();
  UpdateTextData();
}

Box::Level TextBox::GetLevel() const { return kInlineLevel; }

void TextBox::UpdateUsedSize(const LayoutParams& /*layout_params*/) {
  set_used_width(text_width_ + GetLeadingWhiteSpaceWidth() +
                 GetTrailingWhiteSpaceWidth());
  set_used_height(used_line_height_);
}

scoped_ptr<Box> TextBox::TrySplitAt(float available_width,
                                    bool allow_overflow) {
  // Start from the text position when searching for the split position. We do
  // not want to split on leading whitespace. Additionally, as a result of
  // skipping over it, the width of the leading whitespace will need to be
  // removed from the available width.
  available_width -= GetLeadingWhiteSpaceWidth();
  int32 start_position = GetTextStartPosition();
  int32 split_position = start_position;
  float split_width = 0;

  if (paragraph_->CalculateBreakPosition(
          used_font_, start_position, paragraph_end_position_, available_width,
          allow_overflow, &split_position, &split_width)) {
    return SplitAtPosition(split_position, split_width);
  }

  return scoped_ptr<Box>();
}

void TextBox::SplitBidiLevelRuns() {}

scoped_ptr<Box> TextBox::TrySplitAtSecondBidiLevelRun() {
  int32 split_position;
  if (paragraph_->GetNextRunPosition(text_start_position_, &split_position) &&
      split_position < text_end_position_) {
    float pre_split_width = paragraph_->CalculateSubStringWidth(
        used_font_, text_start_position_, split_position);
    return SplitAtPosition(split_position, pre_split_width);
  } else {
    return scoped_ptr<Box>();
  }
}

base::optional<int> TextBox::GetBidiLevel() const {
  return paragraph_->GetBidiLevel(text_start_position_);
}

bool TextBox::IsCollapsed() const {
  return !HasLeadingWhiteSpace() && !HasTrailingWhiteSpace() && !HasText();
}

bool TextBox::HasLeadingWhiteSpace() const {
  return is_paragraph_start_position_white_space_ &&
         !is_leading_white_space_collapsed_;
}

bool TextBox::HasTrailingWhiteSpace() const {
  return is_paragraph_end_position_white_space_ &&
         !is_trailing_white_space_collapsed_;
}

void TextBox::CollapseLeadingWhiteSpace() {
  if (HasLeadingWhiteSpace()) {
    InvalidateUsedWidth();

    is_leading_white_space_collapsed_ = true;
    if (HasTrailingWhiteSpace() && !HasText()) {
      is_trailing_white_space_collapsed_ = true;
    }
  }
}

void TextBox::CollapseTrailingWhiteSpace() {
  if (HasTrailingWhiteSpace()) {
    InvalidateUsedWidth();

    is_trailing_white_space_collapsed_ = true;
    if (HasLeadingWhiteSpace() && !HasText()) {
      is_leading_white_space_collapsed_ = true;
    }
  }
}

bool TextBox::JustifiesLineExistence() const { return HasText(); }

bool TextBox::AffectsBaselineInBlockFormattingContext() const {
  NOTREACHED() << "Should only be called in a block formatting context.";
  return true;
}

float TextBox::GetHeightAboveBaseline() const { return height_above_baseline_; }

void TextBox::AddContentToRenderTree(
    render_tree::CompositionNode::Builder* composition_node_builder,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animations_map_builder) const {
  UNREFERENCED_PARAMETER(node_animations_map_builder);

  // Only add the text node to the render tree if it actually has content.
  if (HasText()) {
    render_tree::ColorRGBA used_color = GetUsedColor(computed_style()->color());

    // Only render the text if it is not completely transparent.
    if (used_color.a() > 0.0f) {
      // The render tree API considers text coordinates to be a position of
      // a baseline, offset the text node accordingly.
      composition_node_builder->AddChild(
          new render_tree::TextNode(GetText(), used_font_, used_color),
          math::TranslateMatrix(GetLeadingWhiteSpaceWidth(),
                                height_above_baseline_));
    }
  }
}

bool TextBox::IsTransformable() const { return false; }

void TextBox::DumpClassName(std::ostream* stream) const {
  *stream << "TextBox ";
}

void TextBox::DumpProperties(std::ostream* stream) const {
  Box::DumpProperties(stream);

  *stream << std::boolalpha
          << "has_leading_white_space=" << HasLeadingWhiteSpace() << " "
          << "has_trailing_white_space=" << HasTrailingWhiteSpace() << " ";
}

void TextBox::DumpChildrenWithIndent(std::ostream* stream, int indent) const {
  Box::DumpChildrenWithIndent(stream, indent);

  DumpIndent(stream, indent);

  *stream << "\"" << GetText() << "\"\n";
}

void TextBox::UpdateFontData() {
  space_width_ = used_font_->GetBounds(" ").width();

  render_tree::FontMetrics font_metrics = used_font_->GetFontMetrics();

  // Below is calculated based on
  // http://www.w3.org/TR/CSS21/visudet.html#leading.

  UsedLineHeightProvider used_line_height_provider(font_metrics);
  computed_style()->line_height()->Accept(&used_line_height_provider);

  // Determine the leading L, where L = "line-height" - AD,
  // AD = A (ascent) + D (descent).
  float leading = used_line_height_provider.used_line_height() -
                  (font_metrics.ascent + font_metrics.descent);

  // The height of the inline box encloses all glyphs and their half-leading
  // on each side and is thus exactly "line-height".
  used_line_height_ = used_line_height_provider.used_line_height();

  // Half the leading is added above ascent (A) and the other half below
  // descent (D), giving the glyph and its leading (L) a total height above
  // the baseline of A' = A + L/2 and a total depth of D' = D + L/2.
  height_above_baseline_ = font_metrics.ascent + leading / 2;
}

void TextBox::UpdateTextData() {
  UpdateParagraphStartPositionWhiteSpace();
  UpdateParagraphEndPositionWhiteSpace();
  UpdateTextWidthIfInvalid();
}

void TextBox::UpdateParagraphStartPositionWhiteSpace() {
  if (paragraph_start_position_ == paragraph_end_position_) {
    is_paragraph_start_position_white_space_ = false;
  } else {
    is_paragraph_start_position_white_space_ =
        paragraph_->IsWhiteSpace(paragraph_start_position_);
  }
}

void TextBox::UpdateParagraphEndPositionWhiteSpace() {
  if (paragraph_start_position_ == paragraph_end_position_) {
    is_paragraph_end_position_white_space_ = false;
  } else {
    is_paragraph_end_position_white_space_ =
        paragraph_->IsWhiteSpace(paragraph_end_position_ - 1);
  }
}

void TextBox::UpdateTextWidthIfInvalid() {
  if (IsTextWidthInvalid()) {
    text_width_ = HasText() ? used_font_->GetBounds(GetText()).width() : 0;
  }
}

std::string TextBox::GetText() const {
  return paragraph_->RetrieveSubString(GetTextStartPosition(),
                                       GetTextEndPosition());
}

scoped_ptr<Box> TextBox::SplitAtPosition(int32 split_start_position,
                                         float pre_split_width) {
  int32 split_end_position = paragraph_end_position_;
  paragraph_end_position_ = split_start_position;

  DCHECK(split_start_position <= split_end_position);

  // Calculate the width of the split section of text.
  float split_text_width = text_width_ - pre_split_width;

  InvalidateUsedWidth();
  text_width_ = pre_split_width;

  // Update the paragraph end position white space now that this text box has
  // a new end position. The start position white space does not need to be
  // updated as it has not changed.
  UpdateParagraphEndPositionWhiteSpace();

  return scoped_ptr<Box>(new TextBox(
      computed_style(), transitions(), used_style_provider(), paragraph_,
      split_start_position, split_end_position, split_text_width));
}

float TextBox::GetLeadingWhiteSpaceWidth() const {
  return HasLeadingWhiteSpace() ? space_width_ : 0;
}

float TextBox::GetTrailingWhiteSpaceWidth() const {
  return HasTrailingWhiteSpace() && HasText() ? space_width_ : 0;
}

bool TextBox::HasText() const {
  return GetTextStartPosition() < GetTextEndPosition();
}

int32 TextBox::GetTextStartPosition() const {
  int32 position = paragraph_start_position_;
  if (is_paragraph_start_position_white_space_) {
    ++position;
  }

  return position;
}

int32 TextBox::GetTextEndPosition() const {
  int32 position = paragraph_end_position_;
  if (is_paragraph_end_position_white_space_) {
    --position;
  }

  return position;
}

bool TextBox::IsTextWidthInvalid() const {
  // If the value is NaN, it won't equal itself.
  return text_width_ != text_width_;
}

}  // namespace layout
}  // namespace cobalt
