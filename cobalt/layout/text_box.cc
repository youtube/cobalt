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

#include "cobalt/layout/text_box.h"

#include <algorithm>
#include <limits>

#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/shadow_value.h"
#include "cobalt/layout/render_tree_animations.h"
#include "cobalt/layout/used_style.h"
#include "cobalt/layout/white_space_processing.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/filter_node.h"
#include "cobalt/render_tree/glyph_buffer.h"
#include "cobalt/render_tree/text_node.h"

namespace cobalt {
namespace layout {

TextBox::TextBox(const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
                     css_computed_style_declaration,
                 const scoped_refptr<Paragraph>& paragraph,
                 int32 text_start_position, int32 text_end_position,
                 bool has_trailing_line_break, bool is_product_of_split,
                 UsedStyleProvider* used_style_provider,
                 LayoutStatTracker* layout_stat_tracker)
    : Box(css_computed_style_declaration, used_style_provider,
          layout_stat_tracker),
      paragraph_(paragraph),
      text_start_position_(text_start_position),
      text_end_position_(text_end_position),
      truncated_text_end_position_(text_end_position),
      previous_truncated_text_end_position_(text_end_position),
      truncated_text_offset_from_left_(0),
      used_font_(used_style_provider->GetUsedFontList(
          css_computed_style_declaration->data()->font_family(),
          css_computed_style_declaration->data()->font_size(),
          css_computed_style_declaration->data()->font_style(),
          css_computed_style_declaration->data()->font_weight())),
      text_has_leading_white_space_(false),
      text_has_trailing_white_space_(false),
      should_collapse_leading_white_space_(false),
      should_collapse_trailing_white_space_(false),
      has_trailing_line_break_(has_trailing_line_break),
      is_product_of_split_(is_product_of_split),
      update_size_results_valid_(false),
      ascent_(0) {
  DCHECK(text_start_position_ <= text_end_position_);

  UpdateTextHasLeadingWhiteSpace();
  UpdateTextHasTrailingWhiteSpace();
}

Box::Level TextBox::GetLevel() const { return kInlineLevel; }

TextBox* TextBox::AsTextBox() { return this; }
const TextBox* TextBox::AsTextBox() const { return this; }

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

LayoutUnit TextBox::GetInlineLevelBoxHeight() const { return line_height_; }

LayoutUnit TextBox::GetInlineLevelTopMargin() const {
  return inline_top_margin_;
}

void TextBox::UpdateContentSizeAndMargins(const LayoutParams& layout_params) {
  // Anonymous boxes do not have margins.
  DCHECK(GetUsedMarginLeftIfNotAuto(computed_style(),
                                    layout_params.containing_block_size)
             ->EqualOrNaN(LayoutUnit()));
  DCHECK(GetUsedMarginTopIfNotAuto(computed_style(),
                                   layout_params.containing_block_size)
             ->EqualOrNaN(LayoutUnit()));
  DCHECK(GetUsedMarginRightIfNotAuto(computed_style(),
                                     layout_params.containing_block_size)
             ->EqualOrNaN(LayoutUnit()));
  DCHECK(GetUsedMarginBottomIfNotAuto(computed_style(),
                                      layout_params.containing_block_size)
             ->EqualOrNaN(LayoutUnit()));

  // The non-collapsible content size only needs to be calculated if
  // |non_collapsible_text_width_| is unset. This indicates that either the
  // width has not previously been calculated for this box, or that the width
  // was invalidated as the result of a split.
  if (!non_collapsible_text_width_) {
    const scoped_refptr<cssom::PropertyValue>& line_height =
        computed_style()->line_height();

    // Factor in all of the fonts needed by the text when generating font
    // metrics if the line height is set to normal:
    // "When an element contains text that is rendered in more than one font,
    // user agents may determine the 'normal' 'line-height' value according to
    // the largest font size."
    //   https://www.w3.org/TR/CSS21/visudet.html#line-height
    bool use_text_fonts_to_generate_font_metrics =
        (line_height == cssom::KeywordValue::GetNormal());

    render_tree::FontVector text_fonts;
    int32 text_start_position = GetNonCollapsibleTextStartPosition();
    non_collapsible_text_width_ =
        HasNonCollapsibleText()
            ? LayoutUnit(used_font_->GetTextWidth(
                  paragraph_->GetTextBuffer() + text_start_position,
                  GetNonCollapsibleTextLength(),
                  paragraph_->IsRTL(text_start_position),
                  use_text_fonts_to_generate_font_metrics ? &text_fonts : NULL))
            : LayoutUnit();

    // The line height values are only calculated when one of two conditions are
    // met:
    //  1. |baseline_offset_from_top_| has not previously been set, meaning that
    //     the line height has never been calculated for this box.
    //  2. |use_text_fonts_to_generate_font_metrics| is true, meaning that the
    //     line height values depend on the content of the text itself. When
    //     this is the case, the line height value is not constant and a split
    //     in the text box can result in the line height values changing.
    if (!baseline_offset_from_top_ || use_text_fonts_to_generate_font_metrics) {
      set_margin_left(LayoutUnit());
      set_margin_top(LayoutUnit());
      set_margin_right(LayoutUnit());
      set_margin_bottom(LayoutUnit());

      render_tree::FontMetrics font_metrics =
          used_font_->GetFontMetrics(text_fonts);

      UsedLineHeightProvider used_line_height_provider(
          font_metrics, computed_style()->font_size());
      line_height->Accept(&used_line_height_provider);
      set_height(LayoutUnit(font_metrics.em_box_height()));
      baseline_offset_from_top_ =
          used_line_height_provider.baseline_offset_from_top();
      line_height_ = used_line_height_provider.used_line_height();
      inline_top_margin_ = used_line_height_provider.half_leading();
      ascent_ = font_metrics.ascent();
    }
  }

  set_width(GetLeadingWhiteSpaceWidth() + *non_collapsible_text_width_ +
            GetTrailingWhiteSpaceWidth());
}

WrapResult TextBox::TryWrapAt(WrapAtPolicy wrap_at_policy,
                              WrapOpportunityPolicy wrap_opportunity_policy,
                              bool is_line_existence_justified,
                              LayoutUnit available_width,
                              bool should_collapse_trailing_white_space) {
  DCHECK(!IsAbsolutelyPositioned());

  bool style_allows_break_word =
      computed_style()->overflow_wrap() == cssom::KeywordValue::GetBreakWord();

  if (!ShouldProcessWrapOpportunityPolicy(wrap_opportunity_policy,
                                          style_allows_break_word)) {
    return kWrapResultNoWrap;
  }

  // Even when the text box's style prevents wrapping, wrapping can still occur
  // before the box if the following requirements are met:
  // - The text box is not the product of a split. If it is, and this box's
  //   style prevents text wrapping, then the previous box also prevents text
  //   wrapping, and no wrap should occur between them.
  // - The line's existence has already been justified. Wrapping cannot occur
  //   prior to that.
  // - Whitespace precedes the text box. This can only occur in the case where
  //   the preceding box allows wrapping, otherwise a no-breaking space will
  //   have been appended (the one exception to this is when this box was the
  //   product of a split, but that case is already handled above).
  if (!DoesAllowTextWrapping(computed_style()->white_space())) {
    if (!is_product_of_split_ && is_line_existence_justified &&
        text_start_position_ > 0 &&
        paragraph_->IsCollapsibleWhiteSpace(text_start_position_ - 1)) {
      return kWrapResultWrapBefore;
    } else {
      return kWrapResultNoWrap;
    }
  }

  // If the line existence is already justified, then leading whitespace can be
  // included in the wrap search, as it provides a wrappable point. If it isn't,
  // then the leading whitespace is skipped, because the line cannot wrap before
  // it is justified.
  int32 start_position = is_line_existence_justified
                             ? text_start_position_
                             : GetNonCollapsibleTextStartPosition();

  int32 wrap_position = GetWrapPosition(
      wrap_at_policy, wrap_opportunity_policy, is_line_existence_justified,
      available_width, should_collapse_trailing_white_space,
      style_allows_break_word, start_position);

  WrapResult wrap_result;
  // Wrapping at the text start position is only allowed when the line's
  // existence is already justified.
  if (wrap_position == text_start_position_ && is_line_existence_justified) {
    wrap_result = kWrapResultWrapBefore;
  } else if (wrap_position > start_position &&
             wrap_position < text_end_position_) {
    SplitAtPosition(wrap_position);
    wrap_result = kWrapResultSplitWrap;
  } else {
    wrap_result = kWrapResultNoWrap;
  }
  return wrap_result;
}

Box* TextBox::GetSplitSibling() const { return split_sibling_; }

bool TextBox::DoesFulfillEllipsisPlacementRequirement() const {
  // This box has non-collapsed text and fulfills the requirement that the first
  // character or inline-level element must appear on the line before ellipsing
  // can occur if it has non-collapsed characters.
  //   https://www.w3.org/TR/css3-ui/#propdef-text-overflow
  return GetNonCollapsedTextStartPosition() < GetNonCollapsedTextEndPosition();
}

void TextBox::DoPreEllipsisPlacementProcessing() {
  previous_truncated_text_end_position_ = truncated_text_end_position_;
  truncated_text_end_position_ = text_end_position_;
}

void TextBox::DoPostEllipsisPlacementProcessing() {
  if (previous_truncated_text_end_position_ != truncated_text_end_position_) {
    InvalidateRenderTreeNodesOfBoxAndAncestors();
  }
}

void TextBox::SplitBidiLevelRuns() {}

bool TextBox::TrySplitAtSecondBidiLevelRun() {
  int32 split_position;
  if (paragraph_->GetNextRunPosition(text_start_position_, &split_position) &&
      split_position < text_end_position_) {
    SplitAtPosition(split_position);
    return true;
  } else {
    return false;
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
  return HasNonCollapsibleText() || has_trailing_line_break_;
}

bool TextBox::HasTrailingLineBreak() const { return has_trailing_line_break_; }

bool TextBox::AffectsBaselineInBlockFormattingContext() const {
  NOTREACHED() << "Should only be called in a block formatting context.";
  return true;
}

LayoutUnit TextBox::GetBaselineOffsetFromTopMarginEdge() const {
  DCHECK(baseline_offset_from_top_);
  return *baseline_offset_from_top_;
}

namespace {
void PopulateBaseStyleForTextNode(
    const scoped_refptr<const cssom::CSSComputedStyleData>& source_style,
    const scoped_refptr<cssom::CSSComputedStyleData>& destination_style) {
  // NOTE: Properties set by PopulateBaseStyleForTextNode() should match the
  // properties used by SetupTextNodeFromStyle().
  destination_style->set_color(source_style->color());
}

void SetupTextNodeFromStyle(
    const scoped_refptr<const cssom::CSSComputedStyleData>& style,
    render_tree::TextNode::Builder* text_node_builder) {
  text_node_builder->color = GetUsedColor(style->color());
}

void AddTextShadows(render_tree::TextNode::Builder* builder,
                    cssom::PropertyListValue* shadow_list) {
  if (shadow_list->value().empty()) {
    return;
  }

  builder->shadows.emplace();
  builder->shadows->reserve(shadow_list->value().size());

  for (size_t s = 0; s < shadow_list->value().size(); ++s) {
    cssom::ShadowValue* shadow_value =
        base::polymorphic_downcast<cssom::ShadowValue*>(
            shadow_list->value()[s].get());

    math::Vector2dF offset(shadow_value->offset_x()->value(),
                           shadow_value->offset_y()->value());

    // Since most of a Gaussian fits within 3 standard deviations from
    // the mean, we setup here the Gaussian blur sigma to be a third of
    // the blur radius.
    float shadow_blur_sigma =
        shadow_value->blur_radius()
            ? GetUsedLength(shadow_value->blur_radius()).toFloat() / 3.0f
            : 0.0f;

    render_tree::ColorRGBA shadow_color = GetUsedColor(shadow_value->color());

    builder->shadows->push_back(
        render_tree::Shadow(offset, shadow_blur_sigma, shadow_color));
  }
}

}  // namespace

void TextBox::RenderAndAnimateContent(
    render_tree::CompositionNode::Builder* border_node_builder,
    ContainerBox* /*stacking_context*/) const {
  if (computed_style()->visibility() != cssom::KeywordValue::GetVisible()) {
    return;
  }

  DCHECK((border_left_width() + padding_left()).EqualOrNaN(LayoutUnit()));
  DCHECK((border_top_width() + padding_top()).EqualOrNaN(LayoutUnit()));

  // Only add the text node to the render tree if it actually has visible
  // content that isn't simply collapsible whitespace and a font isn't loading.
  // The font is treated as transparent if a font is currently being downloaded
  // and hasn't timed out: "In cases where textual content is loaded before
  // downloadable fonts are available, user agents may... render text
  // transparently with fallback fonts to avoid a flash of text using a fallback
  // font. In cases where the font download fails user agents must display text,
  // simply leaving transparent text is considered non-conformant behavior."
  //   https://www.w3.org/TR/css3-fonts/#font-face-loading
  if (HasNonCollapsibleText() && HasVisibleText() && used_font_->IsVisible()) {
    bool is_color_animated =
        animations()->IsPropertyAnimated(cssom::kColorProperty);

    render_tree::ColorRGBA used_color = GetUsedColor(computed_style()->color());

    const scoped_refptr<cssom::PropertyValue>& text_shadow =
        computed_style()->text_shadow();

    // Only render the text if it is not completely transparent, or if the
    // color is animated, in which case it could become non-transparent.
    if (used_color.a() > 0.0f || is_color_animated ||
        text_shadow != cssom::KeywordValue::GetNone()) {
      int32 text_start_position = GetNonCollapsedTextStartPosition();
      int32 text_length = GetVisibleTextLength();

      scoped_refptr<render_tree::GlyphBuffer> glyph_buffer =
          used_font_->CreateGlyphBuffer(
              paragraph_->GetTextBuffer() + text_start_position, text_length,
              paragraph_->IsRTL(text_start_position));

      render_tree::TextNode::Builder text_node_builder(
          math::Vector2dF(truncated_text_offset_from_left_, ascent_),
          glyph_buffer, used_color);

      if (text_shadow != cssom::KeywordValue::GetNone()) {
        cssom::PropertyListValue* shadow_list =
            base::polymorphic_downcast<cssom::PropertyListValue*>(
                computed_style()->text_shadow().get());

        AddTextShadows(&text_node_builder, shadow_list);
      }

      scoped_refptr<render_tree::TextNode> text_node =
          new render_tree::TextNode(text_node_builder);

      // The render tree API considers text coordinates to be a position
      // of a baseline, offset the text node accordingly.
      scoped_refptr<render_tree::Node> node_to_add;
      if (is_color_animated) {
        render_tree::animations::AnimateNode::Builder animate_node_builder;
        AddAnimations<render_tree::TextNode>(
            base::Bind(&PopulateBaseStyleForTextNode),
            base::Bind(&SetupTextNodeFromStyle),
            *css_computed_style_declaration(), text_node,
            &animate_node_builder);
        node_to_add = new render_tree::animations::AnimateNode(
            animate_node_builder, text_node);
      } else {
        node_to_add = text_node;
      }
      border_node_builder->AddChild(node_to_add);
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

  *stream << std::boolalpha << "line_height=" << line_height_ << " "
          << "inline_top_margin=" << inline_top_margin_ << " "
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

void TextBox::DoPlaceEllipsisOrProcessPlacedEllipsis(
    BaseDirection base_direction, LayoutUnit desired_offset,
    bool* is_placement_requirement_met, bool* is_placed,
    LayoutUnit* placed_offset) {
  // If the ellipsis has already been placed, then the text is fully truncated
  // by the ellipsis.
  if (*is_placed) {
    truncated_text_end_position_ = text_start_position_;
    return;
  }

  // Otherwise, the ellipsis is being placed somewhere within this text box.
  *is_placed = true;

  LayoutUnit content_box_start_offset =
      GetContentBoxStartEdgeOffsetFromContainingBlock(base_direction);

  // Determine the available width in the content before to the desired offset.
  // This is the distance from the start edge of the content box to the desired
  // offset.
  LayoutUnit desired_content_offset =
      base_direction == kRightToLeftBaseDirection
          ? content_box_start_offset - desired_offset
          : desired_offset - content_box_start_offset;

  int32 start_position = GetNonCollapsedTextStartPosition();
  int32 end_position = GetNonCollapsedTextEndPosition();
  int32 found_position;
  LayoutUnit found_offset;

  // Attempt to find a break position allowing breaks anywhere within the text,
  // and not simply at soft wrap locations. If the placement requirement has
  // already been satisfied, then the ellipsis can appear anywhere within the
  // text box. Otherwise, it can only appear after the first character
  // (https://www.w3.org/TR/css3-ui/#propdef-text-overflow).
  if (paragraph_->FindBreakPosition(
          used_font_, start_position, end_position, desired_content_offset,
          false, !(*is_placement_requirement_met),
          Paragraph::kBreakPolicyBreakWord, &found_position, &found_offset)) {
    // A usable break position was found. Calculate the placed offset using the
    // the break position's distance from the content box's start edge. In the
    // case where the base direction is right-to-left, the truncated text must
    // be offset to begin after the ellipsis.
    if (base_direction == kRightToLeftBaseDirection) {
      *placed_offset = content_box_start_offset - found_offset;
      truncated_text_offset_from_left_ =
          (*placed_offset - GetContentBoxLeftEdgeOffsetFromContainingBlock())
              .toFloat();
    } else {
      *placed_offset = content_box_start_offset + found_offset;
    }
    truncated_text_end_position_ = found_position;
    // An acceptable break position was not found. If the placement requirement
    // was already met prior to this box, then the ellipsis doesn't require a
    // character from this box to appear prior to its position, so simply place
    // the ellipsis at the start edge of the box and fully truncate the text.
  } else if (is_placement_requirement_met) {
    *placed_offset =
        GetMarginBoxStartEdgeOffsetFromContainingBlock(base_direction);
    truncated_text_end_position_ = text_start_position_;
    // The placement requirement has not already been met. Given that an
    // acceptable break position was not found within the text, the ellipsis can
    // only be placed at the end edge of the box.
  } else {
    *placed_offset =
        GetMarginBoxEndEdgeOffsetFromContainingBlock(base_direction);
  }
}

void TextBox::UpdateTextHasLeadingWhiteSpace() {
  text_has_leading_white_space_ =
      text_start_position_ != text_end_position_ &&
      paragraph_->IsCollapsibleWhiteSpace(text_start_position_) &&
      DoesCollapseWhiteSpace(computed_style()->white_space());
}

void TextBox::UpdateTextHasTrailingWhiteSpace() {
  text_has_trailing_white_space_ =
      !has_trailing_line_break_ && text_start_position_ != text_end_position_ &&
      paragraph_->IsCollapsibleWhiteSpace(text_end_position_ - 1) &&
      DoesCollapseWhiteSpace(computed_style()->white_space());
}

int32 TextBox::GetWrapPosition(WrapAtPolicy wrap_at_policy,
                               WrapOpportunityPolicy wrap_opportunity_policy,
                               bool is_line_existence_justified,
                               LayoutUnit available_width,
                               bool should_collapse_trailing_white_space,
                               bool style_allows_break_word,
                               int32 start_position) {
  Paragraph::BreakPolicy break_policy =
      Paragraph::GetBreakPolicyFromWrapOpportunityPolicy(
          wrap_opportunity_policy, style_allows_break_word);

  int32 wrap_position = -1;
  switch (wrap_at_policy) {
    case kWrapAtPolicyBefore:
      // Wrapping before the box is only permitted when the line's existence is
      // justified.
      if (is_line_existence_justified &&
          paragraph_->IsBreakPosition(text_start_position_, break_policy)) {
        wrap_position = text_start_position_;
      } else {
        wrap_position = -1;
      }
      break;
    case kWrapAtPolicyLastOpportunityWithinWidth: {
      if (is_line_existence_justified) {
        // If the line existence is already justified, then the line can
        // potentially wrap after the box's leading whitespace. However, if that
        // whitespace has been collapsed, then we need to add its width to the
        // available width, because it'll be counted against the available width
        // while searching for the break position, but it won't impact the
        // length of the line.
        if (start_position != GetNonCollapsedTextStartPosition()) {
          available_width += LayoutUnit(used_font_->GetSpaceWidth());
        }
        // If the line's existence isn't already justified, then the line cannot
        // wrap on leading whitespace. Subtract the width of non-collapsed
        // whitespace from the available width, as the search is starting after
        // it.
      } else {
        available_width -= GetLeadingWhiteSpaceWidth();
      }

      // Attempt to find the last break position after the start position that
      // fits within the available width. Overflow is never allowed.
      LayoutUnit wrap_width;
      if (!paragraph_->FindBreakPosition(
              used_font_, start_position, text_end_position_, available_width,
              should_collapse_trailing_white_space, false, break_policy,
              &wrap_position, &wrap_width)) {
        // If no break position is found, but the line existence is already
        // justified, then check for text start position being a break position.
        // Wrapping before the box is permitted when the line's existence is
        // justified.
        if (is_line_existence_justified &&
            paragraph_->IsBreakPosition(text_start_position_, break_policy)) {
          wrap_position = text_start_position_;
        } else {
          wrap_position = -1;
        }
      }
      break;
    }
    case kWrapAtPolicyLastOpportunity:
      wrap_position = paragraph_->GetPreviousBreakPosition(text_end_position_,
                                                           break_policy);
      break;
    case kWrapAtPolicyFirstOpportunity: {
      // If the line is already justified, the wrap can occur at the start
      // position. Otherwise, the wrap cannot occur until after non-collapsible
      // text is included.
      int32 search_start_position =
          is_line_existence_justified ? start_position - 1 : start_position;
      wrap_position =
          paragraph_->GetNextBreakPosition(search_start_position, break_policy);
      break;
    }
  }
  return wrap_position;
}

void TextBox::SplitAtPosition(int32 split_start_position) {
  int32 split_end_position = text_end_position_;
  DCHECK_LT(split_start_position, split_end_position);

  text_end_position_ = split_start_position;
  truncated_text_end_position_ = text_end_position_;

  // The width is no longer valid for this box now that it has been split.
  update_size_results_valid_ = false;
  non_collapsible_text_width_ = base::nullopt;

  const bool kIsProductOfSplitTrue = true;

  scoped_refptr<TextBox> box_after_split(new TextBox(
      css_computed_style_declaration(), paragraph_, split_start_position,
      split_end_position, has_trailing_line_break_, kIsProductOfSplitTrue,
      used_style_provider(), layout_stat_tracker()));

  // Update the split sibling links.
  box_after_split->split_sibling_ = split_sibling_;
  split_sibling_ = box_after_split;

  // TODO: Set the text width of the box after split to
  //       |text_width_ - pre_split_width| to save a call to Skia/HarfBuzz.

  // Pass the trailing line break on to the sibling that retains the trailing
  // portion of the text and reset the value for this text box.
  has_trailing_line_break_ = false;

  // Update the paragraph end position white space now that this text box has
  // a new end position. The start position white space does not need to be
  // updated as it has not changed.
  UpdateTextHasTrailingWhiteSpace();
}

LayoutUnit TextBox::GetLeadingWhiteSpaceWidth() const {
  return HasLeadingWhiteSpace() ? LayoutUnit(used_font_->GetSpaceWidth())
                                : LayoutUnit();
}

LayoutUnit TextBox::GetTrailingWhiteSpaceWidth() const {
  return HasTrailingWhiteSpace() && HasNonCollapsibleText()
             ? LayoutUnit(used_font_->GetSpaceWidth())
             : LayoutUnit();
}

int32 TextBox::GetNonCollapsedTextStartPosition() const {
  return should_collapse_leading_white_space_
             ? GetNonCollapsibleTextStartPosition()
             : text_start_position_;
}

int32 TextBox::GetNonCollapsedTextEndPosition() const {
  return should_collapse_trailing_white_space_
             ? GetNonCollapsibleTextEndPosition()
             : text_end_position_;
}

int32 TextBox::GetNonCollapsibleTextStartPosition() const {
  return text_has_leading_white_space_ ? text_start_position_ + 1
                                       : text_start_position_;
}

int32 TextBox::GetNonCollapsibleTextEndPosition() const {
  return text_has_trailing_white_space_ ? text_end_position_ - 1
                                        : text_end_position_;
}

int32 TextBox::GetNonCollapsibleTextLength() const {
  return GetNonCollapsibleTextEndPosition() -
         GetNonCollapsibleTextStartPosition();
}

bool TextBox::HasNonCollapsibleText() const {
  return GetNonCollapsibleTextLength() > 0;
}

std::string TextBox::GetNonCollapsibleText() const {
  return paragraph_->RetrieveUtf8SubString(GetNonCollapsibleTextStartPosition(),
                                           GetNonCollapsibleTextEndPosition(),
                                           Paragraph::kVisualTextOrder);
}

int32 TextBox::GetVisibleTextEndPosition() const {
  return std::min(GetNonCollapsedTextEndPosition(),
                  truncated_text_end_position_);
}

int32 TextBox::GetVisibleTextLength() const {
  return GetVisibleTextEndPosition() - GetNonCollapsedTextStartPosition();
}

bool TextBox::HasVisibleText() const { return GetVisibleTextLength() > 0; }

std::string TextBox::GetVisibleText() const {
  return paragraph_->RetrieveUtf8SubString(GetNonCollapsedTextStartPosition(),
                                           GetVisibleTextEndPosition(),
                                           Paragraph::kVisualTextOrder);
}

}  // namespace layout
}  // namespace cobalt
