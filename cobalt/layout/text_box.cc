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

#include "cobalt/cssom/keyword_value.h"
#include "cobalt/layout/used_style.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/text_node.h"

namespace cobalt {
namespace layout {

TextBox::TextBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions,
    const UsedStyleProvider* used_style_provider,
    const scoped_refptr<Paragraph>& paragraph, int32 text_start_position,
    int32 text_end_position, bool triggers_line_break)
    : Box(computed_style, transitions, used_style_provider),
      paragraph_(paragraph),
      text_start_position_(text_start_position),
      text_end_position_(text_end_position),
      used_font_(used_style_provider->GetUsedFont(
          computed_style->font_family(), computed_style->font_size(),
          computed_style->font_style(), computed_style->font_weight())),
      text_has_leading_white_space_(false),
      text_has_trailing_white_space_(false),
      should_collapse_leading_white_space_(false),
      should_collapse_trailing_white_space_(false),
      triggers_line_break_(triggers_line_break),
      update_size_results_valid_(false) {
  DCHECK(text_start_position_ <= text_end_position_);

  UpdateTextHasLeadingWhiteSpace();
  UpdateTextHasTrailingWhiteSpace();
}

Box::Level TextBox::GetLevel() const { return kInlineLevel; }

bool TextBox::ValidateUpdateSizeInputs(const LayoutParams& params) {
  // Also take into account mutable local state about (at least) whether white
  // space should be collapsed or not.
  if (Box::ValidateUpdateSizeInputs(params) && update_size_results_valid_) {
    return true;
  } else {
    update_size_results_valid_ = true;
    return false;
  }
}

void TextBox::UpdateContentSizeAndMargins(const LayoutParams& layout_params) {
  // Anonymous boxes do not have margins.
  DCHECK_EQ(0.0f, GetUsedMarginLeftIfNotAuto(
                      computed_style(), layout_params.containing_block_size));
  DCHECK_EQ(0.0f, GetUsedMarginTopIfNotAuto(
                      computed_style(), layout_params.containing_block_size));
  DCHECK_EQ(0.0f, GetUsedMarginRightIfNotAuto(
                      computed_style(), layout_params.containing_block_size));
  DCHECK_EQ(0.0f, GetUsedMarginBottomIfNotAuto(
                      computed_style(), layout_params.containing_block_size));

  set_margin_left(0);
  set_margin_top(0);
  set_margin_right(0);
  set_margin_bottom(0);

  if (!space_width_) {
    space_width_ = used_font_->GetBounds(" ").width();
  }

  float non_collapsible_text_width =
      HasNonCollapsibleText()
          ? used_font_->GetBounds(GetNonCollapsibleText()).width()
          : 0;
  set_width(GetLeadingWhiteSpaceWidth() + non_collapsible_text_width +
            GetTrailingWhiteSpaceWidth());

  if (!baseline_offset_from_top_) {
    UsedLineHeightProvider used_line_height_provider(
        used_font_->GetFontMetrics());
    computed_style()->line_height()->Accept(&used_line_height_provider);
    set_height(used_line_height_provider.used_line_height());
    baseline_offset_from_top_ =
        used_line_height_provider.baseline_offset_from_top();
  }
}

scoped_ptr<Box> TextBox::TrySplitAt(float available_width,
                                    bool allow_overflow) {
  if (!WhiteSpaceStyleAllowsWrapping()) {
    return scoped_ptr<Box>();
  }

  // Start from the text position when searching for the split position. We do
  // not want to split on leading whitespace. Additionally, as a result of
  // skipping over it, the width of the leading whitespace will need to be
  // removed from the available width.
  available_width -= GetLeadingWhiteSpaceWidth();
  int32 start_position = GetNonCollapsibleTextStartPosition();
  int32 split_position = start_position;
  float split_width = 0;

  Paragraph::OverflowWrap overflow_wrap;
  if (computed_style()->overflow_wrap() ==
      cssom::KeywordValue::GetBreakWord()) {
    overflow_wrap = Paragraph::kBreakWordOverflowWrap;
  } else {
    overflow_wrap = Paragraph::kSoftWrapOverflowWrap;
  }

  if (paragraph_->CalculateBreakPosition(
          used_font_, start_position, text_end_position_, available_width,
          allow_overflow, overflow_wrap, &split_position, &split_width)) {
    return SplitAtPosition(split_position);
  }

  return scoped_ptr<Box>();
}

void TextBox::SplitBidiLevelRuns() {}

scoped_ptr<Box> TextBox::TrySplitAtSecondBidiLevelRun() {
  int32 split_position;
  if (paragraph_->GetNextRunPosition(text_start_position_, &split_position) &&
      split_position < text_end_position_) {
    return SplitAtPosition(split_position);
  } else {
    return scoped_ptr<Box>();
  }
}

base::optional<int> TextBox::GetBidiLevel() const {
  return paragraph_->GetBidiLevel(text_start_position_);
}

void TextBox::SetShouldCollapseLeadingWhiteSpace(
    bool should_collapse_leading_white_space) {
  if (should_collapse_leading_white_space_ !=
      should_collapse_leading_white_space) {
    should_collapse_leading_white_space_ = should_collapse_leading_white_space;
    update_size_results_valid_ = false;
  }
}

void TextBox::SetShouldCollapseTrailingWhiteSpace(
    bool should_collapse_trailing_white_space) {
  if (should_collapse_trailing_white_space_ !=
      should_collapse_trailing_white_space) {
    should_collapse_trailing_white_space_ =
        should_collapse_trailing_white_space;
    update_size_results_valid_ = false;
  }
}

bool TextBox::IsCollapsed() const {
  return !HasLeadingWhiteSpace() && !HasTrailingWhiteSpace() &&
         !HasNonCollapsibleText();
}

bool TextBox::HasLeadingWhiteSpace() const {
  return text_has_leading_white_space_ &&
         !should_collapse_leading_white_space_ &&
         (HasNonCollapsibleText() || !should_collapse_trailing_white_space_);
}

bool TextBox::HasTrailingWhiteSpace() const {
  return text_has_trailing_white_space_ &&
         !should_collapse_trailing_white_space_ &&
         (HasNonCollapsibleText() || !should_collapse_leading_white_space_);
}

bool TextBox::JustifiesLineExistence() const {
  return HasNonCollapsibleText() || triggers_line_break_;
}

bool TextBox::DoesTriggerLineBreak() const { return triggers_line_break_; }

bool TextBox::AffectsBaselineInBlockFormattingContext() const {
  NOTREACHED() << "Should only be called in a block formatting context.";
  return true;
}

float TextBox::GetBaselineOffsetFromTopMarginEdge() const {
  return *baseline_offset_from_top_;
}

void TextBox::RenderAndAnimateContent(
    render_tree::CompositionNode::Builder* border_node_builder,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animations_map_builder) const {
  UNREFERENCED_PARAMETER(node_animations_map_builder);

  DCHECK_EQ(0, border_left_width() + padding_left());
  DCHECK_EQ(0, border_top_width() + padding_top());

  // Only add the text node to the render tree if it actually has content.
  if (HasNonCollapsibleText()) {
    render_tree::ColorRGBA used_color = GetUsedColor(computed_style()->color());

    // Only render the text if it is not completely transparent.
    if (used_color.a() > 0.0f) {
      // The render tree API considers text coordinates to be a position of
      // a baseline, offset the text node accordingly.
      border_node_builder->AddChild(
          new render_tree::TextNode(GetNonCollapsibleText(), used_font_,
                                    used_color),
          math::TranslateMatrix(GetLeadingWhiteSpaceWidth(),
                                *baseline_offset_from_top_));
    }
  }
}

bool TextBox::IsTransformable() const { return false; }

#ifdef COBALT_BOX_DUMP_ENABLED

void TextBox::DumpClassName(std::ostream* stream) const {
  *stream << "TextBox ";
}

void TextBox::DumpProperties(std::ostream* stream) const {
  Box::DumpProperties(stream);

  *stream << "text_start=" << text_start_position_ << " "
          << "text_end=" << text_end_position_ << " ";

  *stream << std::boolalpha
          << "has_leading_white_space=" << HasLeadingWhiteSpace() << " "
          << "has_trailing_white_space=" << HasTrailingWhiteSpace() << " "
          << std::noboolalpha;

  *stream << "bidi_level=" << paragraph_->GetBidiLevel(text_start_position_)
          << " ";
}

void TextBox::DumpChildrenWithIndent(std::ostream* stream, int indent) const {
  Box::DumpChildrenWithIndent(stream, indent);

  DumpIndent(stream, indent);

  *stream << "\"" << GetNonCollapsibleText() << "\"\n";
}

#endif  // COBALT_BOX_DUMP_ENABLED

bool TextBox::WhiteSpaceStyleAllowsCollapsing() {
  return computed_style()->white_space() != cssom::KeywordValue::GetPre();
}

bool TextBox::WhiteSpaceStyleAllowsWrapping() {
  return computed_style()->white_space() != cssom::KeywordValue::GetPre() &&
         computed_style()->white_space() != cssom::KeywordValue::GetNoWrap();
}

void TextBox::UpdateTextHasLeadingWhiteSpace() {
  if (WhiteSpaceStyleAllowsCollapsing()) {
    text_has_leading_white_space_ =
        text_start_position_ != text_end_position_ &&
        paragraph_->IsSpace(text_start_position_);
  }
}

void TextBox::UpdateTextHasTrailingWhiteSpace() {
  if (WhiteSpaceStyleAllowsCollapsing()) {
    text_has_trailing_white_space_ =
        text_start_position_ != text_end_position_ &&
        paragraph_->IsSpace(text_end_position_ - 1);
  }
}

scoped_ptr<Box> TextBox::SplitAtPosition(int32 split_start_position) {
  int32 split_end_position = text_end_position_;
  DCHECK_LE(split_start_position, split_end_position);

  text_end_position_ = split_start_position;

  // Update the paragraph end position white space now that this text box has
  // a new end position. The start position white space does not need to be
  // updated as it has not changed.
  UpdateTextHasTrailingWhiteSpace();

  scoped_ptr<Box> box_after_split(new TextBox(
      computed_style(), transitions(), used_style_provider(), paragraph_,
      split_start_position, split_end_position, triggers_line_break_));

  // TODO(***REMOVED***): Set the text width of the box after split to
  //               |text_width_ - pre_split_width| to save a call
  //               to Skia/HarfBuzz.

  // Pass the line break trigger on to the sibling that retains the trailing
  // portion of the text and reset the value for this text box.
  triggers_line_break_ = false;

  return box_after_split.Pass();
}

float TextBox::GetLeadingWhiteSpaceWidth() const {
  return HasLeadingWhiteSpace() ? *space_width_ : 0;
}

float TextBox::GetTrailingWhiteSpaceWidth() const {
  return HasTrailingWhiteSpace() && HasNonCollapsibleText() ? *space_width_ : 0;
}

int32 TextBox::GetNonCollapsibleTextStartPosition() const {
  return text_has_leading_white_space_ ? text_start_position_ + 1
                                       : text_start_position_;
}

int32 TextBox::GetNonCollapsibleTextEndPosition() const {
  return text_has_trailing_white_space_ ? text_end_position_ - 1
                                        : text_end_position_;
}

bool TextBox::HasNonCollapsibleText() const {
  return GetNonCollapsibleTextStartPosition() <
         GetNonCollapsibleTextEndPosition();
}

std::string TextBox::GetNonCollapsibleText() const {
  return paragraph_->RetrieveUtf8SubString(GetNonCollapsibleTextStartPosition(),
                                           GetNonCollapsibleTextEndPosition());
}

}  // namespace layout
}  // namespace cobalt
