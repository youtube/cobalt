// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/line_breaker.h"

#include "base/containers/adapters.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/inline/inline_break_token.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_segment.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/line_break_candidate.h"
#include "third_party/blink/renderer/core/layout/inline/line_info.h"
#include "third_party/blink/renderer/core/layout/inline/ruby_utils.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/layout/svg/resolved_text_layout_attributes_iterator.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_text_content_element.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shaping_line_breaker.h"
#include "third_party/blink/renderer/platform/text/bidi_paragraph.h"
#include "third_party/blink/renderer/platform/text/character.h"

namespace blink {

namespace {

inline LineBreakStrictness StrictnessFromLineBreak(LineBreak line_break) {
  switch (line_break) {
    case LineBreak::kAuto:
    case LineBreak::kAfterWhiteSpace:
    case LineBreak::kAnywhere:
      return LineBreakStrictness::kDefault;
    case LineBreak::kNormal:
      return LineBreakStrictness::kNormal;
    case LineBreak::kStrict:
      return LineBreakStrictness::kStrict;
    case LineBreak::kLoose:
      return LineBreakStrictness::kLoose;
  }
  NOTREACHED();
  return LineBreakStrictness::kDefault;
}

// Returns smallest negative left and right bearing in `box_fragment`.
// This function is used for calculating side bearing.
LineBoxStrut ComputeNegativeSideBearings(
    const NGPhysicalBoxFragment& box_fragment) {
  const auto get_shape_result =
      [](const InlineCursor cursor) -> const ShapeResultView* {
    if (!cursor)
      return nullptr;
    const FragmentItem& item = *cursor.CurrentItem();
    if (item.Type() != FragmentItem::kText &&
        item.Type() != FragmentItem::kGeneratedText) {
      return nullptr;
    }
    if (item.IsFlowControl())
      return nullptr;
    return item.TextShapeResult();
  };

  LineBoxStrut side_bearing;

  for (InlineCursor cursor(box_fragment); cursor; cursor.MoveToNextLine()) {
    // Take left/right bearing from the first/last child in the line if it has
    // `ShapeResult`. The first/last child can be non text item, e.g. image.
    // Note: Items in the line are in visual order. So, first=left, last=right.
    //
    // Example: If we have three text item "[", "T", "]", we should take left
    // baring from "[" and right bearing from "]". The text ink bounds of "T"
    // is not involved with side bearing calculation.
    DCHECK(cursor.Current().IsLineBox());

    // `gfx::RectF` returned from `ShapeResult::ComputeInkBounds()` is in
    // text origin coordinate aka baseline. Y-coordinate of points above
    // baseline are negative.
    //
    //  Text Ink Bounds:
    //   * left bearing = text_ink_bounds.X()
    //   * right bearing = width - text_ink_bounds.InlineEndOffset()
    //
    //          <--> left bearing (positive)
    //          ...+---------+
    //          ...|*********|..<
    //          ...|....*....|..<
    //          ...|....*....|<-> right bearing (positive)
    //          ...|....*....|..<
    //          ...|....*....|..<
    //          >..+----*----+..< baseline
    //          ^text origin
    //          <---------------> width/advance
    //
    //            left bearing (negative)
    //          <-->          <--> right bearing (negative)
    //          +----------------+
    //          |... *****..*****|
    //          |......*.....*<..|
    //          |.....*.....*.<..|
    //          |....*******..<..|
    //          |...*.....*...<..|
    //          |..*.....*....<..|
    //          +****..*****..<..+
    //             ^ text origin
    //             <----------> width/advance
    //
    // When `FragmentItem` has `ShapeTesult`, its `rect` is
    //    * `rect.offset.left = X`
    //    * `rect.size.width  = shape_result.SnappedWidth() // advance
    // where `X` is the original item offset.
    // For the initial letter text, its `rect` is[1]
    //    * `rect.offset.left = X - text_ink_bounds.X()`
    //    * `rect.size.width  = text_ink_bounds.Width()`
    // [1] https://drafts.csswg.org/css-inline/#initial-letter-box-size
    // Sizeing the Initial Letter Box
    InlineCursor child_at_left_edge = cursor;
    child_at_left_edge.MoveToFirstChild();
    if (auto* shape_result = get_shape_result(child_at_left_edge)) {
      const LayoutUnit left_bearing =
          LogicalRect::EnclosingRect(shape_result->ComputeInkBounds())
              .offset.inline_offset;
      side_bearing.inline_start =
          std::min(side_bearing.inline_start, left_bearing);
    }

    InlineCursor child_at_right_edge = cursor;
    child_at_right_edge.MoveToLastChild();
    if (auto* shape_result = get_shape_result(child_at_right_edge)) {
      const LayoutUnit width = shape_result->SnappedWidth();
      const LogicalRect text_ink_bounds =
          LogicalRect::EnclosingRect(shape_result->ComputeInkBounds());
      const LayoutUnit right_bearing =
          width - text_ink_bounds.InlineEndOffset();
      side_bearing.inline_end =
          std::min(side_bearing.inline_end, right_bearing);
    }
  }

  return side_bearing;
}

// This rule comes from the spec[1].
// Note: We don't apply inline kerning for vertical writing mode with text
// orientation other than `sideways` because characters are laid out vertically.
// [1] https://drafts.csswg.org/css-inline/#initial-letter-inline-position
bool ShouldApplyInlineKerning(const NGPhysicalBoxFragment& box_fragment) {
  if (!box_fragment.Borders().IsZero() || !box_fragment.Padding().IsZero())
    return false;
  const ComputedStyle& style = box_fragment.Style();
  return style.IsHorizontalWritingMode() ||
         style.GetTextOrientation() == ETextOrientation::kSideways;
}

// CSS-defined white space characters, excluding the newline character.
// In most cases, the line breaker consider break opportunities are before
// spaces because it handles trailing spaces differently from other normal
// characters, but breaking before newline characters is not desired.
inline bool IsBreakableSpace(UChar c) {
  return c == kSpaceCharacter || c == kTabulationCharacter;
}

inline bool IsBreakableSpaceOrOtherSeparator(UChar c) {
  return IsBreakableSpace(c) || Character::IsOtherSpaceSeparator(c);
}

inline bool IsAllBreakableSpaces(const String& string,
                                 unsigned start,
                                 unsigned end) {
  DCHECK_GE(end, start);
  return StringView(string, start, end - start)
      .IsAllSpecialCharacters<IsBreakableSpace>();
}

inline LayoutUnit HyphenAdvance(const ComputedStyle& style,
                                bool is_ltr,
                                const HyphenResult& hyphen_result,
                                absl::optional<LayoutUnit>& cache) {
  if (cache) {
    return *cache;
  }
  const LayoutUnit size = hyphen_result ? hyphen_result.InlineSize()
                                        : HyphenResult(style).InlineSize();
  const LayoutUnit advance = is_ltr ? size : -size;
  cache = advance;
  return advance;
}

// True if the item is "trailable". Trailable items should be included in the
// line if they are after the soft wrap point.
//
// Note that some items are ambiguous; e.g., text is trailable if it has leading
// spaces, and open tags are trailable if spaces follow. This function returns
// true for such cases.
inline bool IsTrailableItemType(InlineItem::InlineItemType type) {
  return type != InlineItem::kAtomicInline &&
         type != InlineItem::kOutOfFlowPositioned &&
         type != InlineItem::kInitialLetterBox &&
         type != InlineItem::kListMarker;
}

inline bool CanBreakAfterLast(const InlineItemResults& item_results) {
  return !item_results.empty() && item_results.back().can_break_after;
}

inline bool ShouldCreateLineBox(const InlineItemResults& item_results) {
  return !item_results.empty() && item_results.back().should_create_line_box;
}

inline bool HasUnpositionedFloats(const InlineItemResults& item_results) {
  return !item_results.empty() && item_results.back().has_unpositioned_floats;
}

LayoutUnit ComputeInlineEndSize(const NGConstraintSpace& space,
                                const ComputedStyle* style) {
  DCHECK(style);
  BoxStrut margins = ComputeMarginsForSelf(space, *style);
  BoxStrut borders = ComputeBordersForInline(*style);
  BoxStrut paddings = ComputePadding(space, *style);

  return margins.inline_end + borders.inline_end + paddings.inline_end;
}

bool NeedsAccurateEndPosition(const InlineItem& line_end_item) {
  DCHECK(line_end_item.Type() == InlineItem::kText ||
         line_end_item.Type() == InlineItem::kControl);
  DCHECK(line_end_item.Style());
  const ComputedStyle& line_end_style = *line_end_item.Style();
  return line_end_style.HasBoxDecorationBackground() ||
         line_end_style.HasAppliedTextDecorations();
}

inline bool NeedsAccurateEndPosition(const LineInfo& line_info,
                                     const InlineItem& line_end_item) {
  return line_info.NeedsAccurateEndPosition() ||
         NeedsAccurateEndPosition(line_end_item);
}

inline void ComputeCanBreakAfter(InlineItemResult* item_result,
                                 bool auto_wrap,
                                 const LazyLineBreakIterator& break_iterator) {
  item_result->can_break_after =
      auto_wrap && break_iterator.IsBreakable(item_result->EndOffset());
}

inline void RemoveLastItem(LineInfo* line_info) {
  InlineItemResults* item_results = line_info->MutableResults();
  DCHECK_GT(item_results->size(), 0u);
  item_results->Shrink(item_results->size() - 1);
}

// To correctly determine if a float is allowed to be on the same line as its
// content, we need to determine if it has any ancestors with inline-end
// padding, border, or margin.
// The inline-end size from all of these ancestors contribute to the "used
// size" of the float, and may cause the float to be pushed down.
LayoutUnit ComputeFloatAncestorInlineEndSize(
    const NGConstraintSpace& space,
    const HeapVector<InlineItem>& items,
    wtf_size_t item_index) {
  LayoutUnit inline_end_size;
  for (const InlineItem *cur = items.begin() + item_index, *end = items.end();
       cur != end; ++cur) {
    const InlineItem& item = *cur;

    if (item.Type() == InlineItem::kCloseTag) {
      inline_end_size += ComputeInlineEndSize(space, item.Style());
      continue;
    }

    // For this calculation, any open tag (even if its empty) stops this
    // calculation, and allows the float to appear on the same line. E.g.
    // <span style="padding-right: 20px;"><f></f><span></span></span>
    //
    // Any non-empty item also allows the float to be on the same line.
    if (item.Type() == InlineItem::kOpenTag || !item.IsEmptyItem()) {
      break;
    }
  }
  return inline_end_size;
}

// See LineBreaker::SplitTextIntoSegments().
void CollectCharIndex(void* context,
                      unsigned char_index,
                      Glyph,
                      gfx::Vector2dF,
                      float,
                      bool,
                      CanvasRotationInVertical,
                      const SimpleFontData*) {
  auto* index_list = static_cast<Vector<unsigned>*>(context);
  wtf_size_t size = index_list->size();
  if (size > 0 && index_list->at(size - 1) == char_index)
    return;
  index_list->push_back(char_index);
}

float ComputeWordWidth(const ShapeResult& shape_result,
                       wtf_size_t start_offset,
                       wtf_size_t end_offset) {
  const wtf_size_t offset_adjust = shape_result.StartIndex();
  const float start_position =
      shape_result.CachedPositionForOffset(start_offset - offset_adjust);
  const float end_position =
      shape_result.CachedPositionForOffset(end_offset - offset_adjust);
  return IsLtr(shape_result.Direction()) ? end_position - start_position
                                         : start_position - end_position;
}

inline LayoutTextCombine* MayBeTextCombine(const InlineItem* item) {
  if (!item)
    return nullptr;
  return DynamicTo<LayoutTextCombine>(item->GetLayoutObject());
}

}  // namespace

inline bool LineBreaker::ShouldAutoWrap(const ComputedStyle& style) const {
  //  TODO(crbug.com/366553): SVG <text> should not be auto_wrap_ for now.
  if (UNLIKELY(is_svg_text_))
    return false;
  // Combine text should not cause line break.
  if (UNLIKELY(is_text_combine_))
    return false;
  // TODO(crbug.com/1276900): Once we implement multiple line initial letter,
  // we should allow auto wrap. Below example causes multiple lines text in
  // initial letter box.
  //   <style>
  //    p::.first-letter { line-break: anywhere; }
  //    p { width: 0px; }
  //  </style>
  //  <p>(A) punctuation characters can be part of ::first-letter.</p>
  if (UNLIKELY(is_initial_letter_box_))
    return false;
  return style.ShouldWrapLine();
}

void LineBreaker::UpdateAvailableWidth() {
  LayoutUnit available_width;
  if (UNLIKELY(override_available_width_)) {
    available_width = override_available_width_;
  } else {
    available_width = line_opportunity_.AvailableInlineSize();
  }
  // Make sure it's at least the initial size, which is usually 0 but not so
  // when `box-decoration-break: clone`.
  available_width =
      std::max(available_width, cloned_box_decorations_initial_size_);
  // Available width must be smaller than |LayoutUnit::Max()| so that the
  // position can be larger.
  available_width = std::min(available_width, LayoutUnit::NearlyMax());
  available_width_ = available_width;
}

LineBreaker::LineBreaker(InlineNode node,
                         LineBreakerMode mode,
                         const NGConstraintSpace& space,
                         const LineLayoutOpportunity& line_opportunity,
                         const LeadingFloats& leading_floats,
                         const InlineBreakToken* break_token,
                         const NGColumnSpannerPath* column_spanner_path,
                         ExclusionSpace* exclusion_space)
    : line_opportunity_(line_opportunity),
      node_(node),
      mode_(mode),
      is_initial_letter_box_(node.IsInitialLetterBox()),
      is_svg_text_(node.IsSvgText()),
      is_text_combine_(node.IsTextCombine()),
      is_first_formatted_line_(
          (!break_token || break_token->Start().IsZero()) &&
          node.CanContainFirstFormattedLine()),
      use_first_line_style_(is_first_formatted_line_ &&
                            node.UseFirstLineStyle()),
      sticky_images_quirk_(mode != LineBreakerMode::kContent &&
                           node.IsStickyImagesQuirkForContentSize()),
      items_data_(node.ItemsData(use_first_line_style_)),
      text_content_(
          !sticky_images_quirk_
              ? items_data_.text_content
              : InlineNode::TextContentForStickyImagesQuirk(items_data_)),
      constraint_space_(space),
      exclusion_space_(exclusion_space),
      break_token_(break_token),
      column_spanner_path_(column_spanner_path),
      break_iterator_(text_content_),
      shaper_(text_content_),
      spacing_(text_content_, is_svg_text_),
      leading_floats_(leading_floats),
      base_direction_(node_.BaseDirection()) {
  UpdateAvailableWidth();
  if (is_svg_text_) {
    const auto& char_data_list = node_.SvgCharacterDataList();
    if (node_.SvgTextPathRangeList().empty() &&
        node_.SvgTextLengthRangeList().empty() &&
        (char_data_list.empty() ||
         (char_data_list.size() == 1 && char_data_list[0].first == 0))) {
      needs_svg_segmentation_ = false;
    } else {
      needs_svg_segmentation_ = true;
      svg_resolved_iterator_ =
          std::make_unique<ResolvedTextLayoutAttributesIterator>(
              char_data_list);
    }
  }

  if (!break_token)
    return;

  const ComputedStyle* line_initial_style = break_token->Style();
  if (UNLIKELY(!line_initial_style)) {
    // Usually an inline break token has the line initial style, but class C
    // breaks and last-resort breaks require a break token to start from the
    // beginning of the block. In that case, the line is still the first
    // formatted line, and the line initial style should be computed from the
    // containing block.
    DCHECK_EQ(break_token->StartItemIndex(), 0u);
    DCHECK_EQ(break_token->StartTextOffset(), 0u);
    DCHECK(!break_token->IsForcedBreak());
    DCHECK_EQ(current_, break_token->Start());
    DCHECK_EQ(is_after_forced_break_, break_token->IsForcedBreak());
    return;
  }

  current_ = break_token->Start();
  break_iterator_.SetStartOffset(current_.text_offset);
  is_after_forced_break_ = break_token->IsForcedBreak();
  items_data_.AssertOffset(current_);
  SetCurrentStyle(*line_initial_style);
}

LineBreaker::~LineBreaker() = default;

void LineBreaker::SetLineOpportunity(
    const LineLayoutOpportunity& line_opportunity) {
  line_opportunity_ = line_opportunity;
  UpdateAvailableWidth();
}

void LineBreaker::OverrideAvailableWidth(LayoutUnit available_width) {
  DCHECK(available_width);
  override_available_width_ = available_width;
  UpdateAvailableWidth();
}

void LineBreaker::SetBreakAt(const LineBreakPoint& offset) {
  break_at_ = offset;
  OverrideAvailableWidth(LayoutUnit::NearlyMax());
}

inline InlineItemResult* LineBreaker::AddItem(const InlineItem& item,
                                              unsigned end_offset,
                                              LineInfo* line_info) {
  DCHECK_EQ(&item, &items_data_.items[current_.item_index]);
  DCHECK_GE(current_.text_offset, item.StartOffset());
  DCHECK_GE(end_offset, current_.text_offset);
  DCHECK_LE(end_offset, item.EndOffset());
  if (UNLIKELY(item.IsTextCombine()))
    line_info->SetHaveTextCombineItem();
  InlineItemResults* item_results = line_info->MutableResults();
  return &item_results->emplace_back(
      &item, current_.item_index,
      TextOffsetRange(current_.text_offset, end_offset),
      break_anywhere_if_overflow_, ShouldCreateLineBox(*item_results),
      HasUnpositionedFloats(*item_results));
}

inline InlineItemResult* LineBreaker::AddItem(const InlineItem& item,
                                              LineInfo* line_info) {
  return AddItem(item, item.EndOffset(), line_info);
}

InlineItemResult* LineBreaker::AddEmptyItem(const InlineItem& item,
                                            LineInfo* line_info) {
  InlineItemResult* item_result =
      AddItem(item, current_.text_offset, line_info);

  // Prevent breaking before an empty item, but allow to break after if the
  // previous item had `can_break_after`.
  DCHECK(!item_result->can_break_after);
  if (line_info->Results().size() >= 2) {
    InlineItemResult* last_item_result = std::prev(item_result);
    if (last_item_result->can_break_after) {
      last_item_result->can_break_after = false;
      item_result->can_break_after = true;
    }
  }
  return item_result;
}

// Call |HandleOverflow()| if the position is beyond the available space.
inline bool LineBreaker::HandleOverflowIfNeeded(LineInfo* line_info) {
  if (state_ == LineBreakState::kContinue && !CanFitOnLine()) {
    HandleOverflow(line_info);
    return true;
  }
  return false;
}

void LineBreaker::SetIntrinsicSizeOutputs(
    MaxSizeCache* max_size_cache,
    bool* depends_on_block_constraints_out) {
  DCHECK_NE(mode_, LineBreakerMode::kContent);
  DCHECK(max_size_cache);
  max_size_cache_ = max_size_cache;
  depends_on_block_constraints_out_ = depends_on_block_constraints_out;
}

// Compute the base direction for bidi algorithm for this line.
void LineBreaker::ComputeBaseDirection() {
  // If 'unicode-bidi' is not 'plaintext', use the base direction of the block.
  if (node_.Style().GetUnicodeBidi() != UnicodeBidi::kPlaintext)
    return;

  const String& text = Text();
  if (text.Is8Bit())
    return;

  // If 'unicode-bidi: plaintext', compute the base direction for each
  // "paragraph" (separated by forced break.)
  wtf_size_t start_offset;
  if (previous_line_had_forced_break_) {
    start_offset = current_.text_offset;
  } else {
    // If this "paragraph" is at the beginning of the block, use
    // |node_.BaseDirection()|.
    if (!current_.text_offset) {
      return;
    }
    start_offset =
        text.ReverseFind(kNewlineCharacter, current_.text_offset - 1);
    if (start_offset == kNotFound)
      return;
    ++start_offset;
  }

  // LTR when no strong characters because `plaintext` uses P2 and P3 of UAX#9:
  // https://w3c.github.io/csswg-drafts/css-writing-modes-3/#valdef-unicode-bidi-plaintext
  // which sets to LTR if no strong characters.
  // https://unicode.org/reports/tr9/#P3
  base_direction_ = BidiParagraph::BaseDirectionForStringOrLtr(
      StringView(text, start_offset),
      // For CSS processing, line feed (U+000A) is treated as a segment break.
      // https://w3c.github.io/csswg-drafts/css-text-3/#segment-break
      Character::IsLineFeed);
}

void LineBreaker::RecalcClonedBoxDecorations() {
  cloned_box_decorations_count_ = 0u;
  cloned_box_decorations_initial_size_ = LayoutUnit();
  cloned_box_decorations_end_size_ = LayoutUnit();
  has_cloned_box_decorations_ = false;

  // Compute which tags are not closed at |current_.item_index|.
  InlineItemsData::OpenTagItems open_items;
  items_data_.GetOpenTagItems(current_.item_index, &open_items);

  for (const InlineItem* item : open_items) {
    if (item->Style()->BoxDecorationBreak() == EBoxDecorationBreak::kClone) {
      has_cloned_box_decorations_ = true;
      disable_score_line_break_ = true;
      ++cloned_box_decorations_count_;
      InlineItemResult item_result;
      ComputeOpenTagResult(*item, constraint_space_, is_svg_text_,
                           &item_result);
      cloned_box_decorations_initial_size_ += item_result.inline_size;
      cloned_box_decorations_end_size_ += item_result.margins.inline_end +
                                          item_result.borders.inline_end +
                                          item_result.padding.inline_end;
    }
  }
  // Advance |position_| by the initial size so that the tab position can
  // accommodate cloned box decorations.
  position_ += cloned_box_decorations_initial_size_;
  // |cloned_box_decorations_initial_size_| may affect available width.
  UpdateAvailableWidth();
  DCHECK_GE(available_width_, cloned_box_decorations_initial_size_);
}

// Add a hyphen string to the |InlineItemResult|.
//
// This function changes |InlineItemResult::inline_size|, but does not change
// |position_|
LayoutUnit LineBreaker::AddHyphen(InlineItemResults* item_results,
                                  wtf_size_t index,
                                  InlineItemResult* item_result) {
  DCHECK(!HasHyphen());
  DCHECK_EQ(index,
            static_cast<wtf_size_t>(item_result - item_results->begin()));
  DCHECK_LT(index, item_results->size());
  hyphen_index_ = index;

  if (!item_result->hyphen) {
    item_result->ShapeHyphen();
    has_any_hyphens_ = true;
  }
  DCHECK(item_result->hyphen);
  DCHECK(has_any_hyphens_);

  const LayoutUnit hyphen_inline_size = item_result->hyphen.InlineSize();
  item_result->inline_size += hyphen_inline_size;
  return hyphen_inline_size;
}

LayoutUnit LineBreaker::AddHyphen(InlineItemResults* item_results,
                                  wtf_size_t index) {
  InlineItemResult* item_result = &(*item_results)[index];
  DCHECK(item_result->item);
  return AddHyphen(item_results, index, item_result);
}

LayoutUnit LineBreaker::AddHyphen(InlineItemResults* item_results,
                                  InlineItemResult* item_result) {
  return AddHyphen(
      item_results,
      base::checked_cast<wtf_size_t>(item_result - item_results->begin()),
      item_result);
}

// Remove the hyphen string from the |InlineItemResult|.
//
// This function changes |InlineItemResult::inline_size|, but does not change
// |position_|
LayoutUnit LineBreaker::RemoveHyphen(InlineItemResults* item_results) {
  DCHECK(HasHyphen());
  InlineItemResult* item_result = &(*item_results)[*hyphen_index_];
  DCHECK(item_result->hyphen);
  const LayoutUnit hyphen_inline_size = item_result->hyphen.InlineSize();
  item_result->inline_size -= hyphen_inline_size;
  // |hyphen_string| and |hyphen_shape_result| may be reused when rewinded.
  hyphen_index_.reset();
  return hyphen_inline_size;
}

// Add a hyphen string to the last inflow item in |item_results| if it is
// hyphenated. This can restore the hyphenation state after rewind.
void LineBreaker::RestoreLastHyphen(InlineItemResults* item_results) {
  DCHECK(!hyphen_index_);
  DCHECK(has_any_hyphens_);
  for (InlineItemResult& item_result : base::Reversed(*item_results)) {
    DCHECK(item_result.item);
    if (item_result.hyphen) {
      AddHyphen(item_results, &item_result);
      return;
    }
    const InlineItem& item = *item_result.item;
    if (item.Type() == InlineItem::kText ||
        item.Type() == InlineItem::kAtomicInline) {
      return;
    }
  }
}

// Set the final hyphenation results to |item_results|.
void LineBreaker::FinalizeHyphen(InlineItemResults* item_results) {
  DCHECK(HasHyphen());
  InlineItemResult* item_result = &(*item_results)[*hyphen_index_];
  DCHECK(item_result->hyphen);
  item_result->is_hyphenated = true;
}

// Initialize internal states for the next line.
void LineBreaker::PrepareNextLine(LineInfo* line_info) {
  line_info->Reset();

  const InlineItemResults& item_results = line_info->Results();
  DCHECK(item_results.empty());

  if (!current_.IsZero()) {
    // We're past the first line
    previous_line_had_forced_break_ = is_after_forced_break_;
    is_after_forced_break_ = false;
    is_first_formatted_line_ = false;
    use_first_line_style_ = false;
  }

  line_info->SetStart(current_);
  line_info->SetLineStyle(node_, items_data_, use_first_line_style_);

  DCHECK(!line_info->TextIndent());
  if (line_info->LineStyle().ShouldUseTextIndent(is_first_formatted_line_)) {
    const Length& length = line_info->LineStyle().TextIndent();
    LayoutUnit maximum_value;
    // Ignore percentages (resolve to 0) when calculating min/max intrinsic
    // sizes.
    if (length.IsPercentOrCalc() && mode_ == LineBreakerMode::kContent) {
      maximum_value = constraint_space_.AvailableSize().inline_size;
    }
    line_info->SetTextIndent(MinimumValueForLength(length, maximum_value));
  }

  // Set the initial style of this line from the line style, if the style from
  // the end of previous line is not available. Example:
  //   <p>...<span>....</span></p>
  // When the line wraps in <span>, the 2nd line needs to start with the style
  // of the <span>.
  override_break_anywhere_ = false;
  disable_phrase_ = false;
  disable_score_line_break_ = false;
  if (!current_style_)
    SetCurrentStyle(line_info->LineStyle());
  ComputeBaseDirection();
  line_info->SetBaseDirection(base_direction_);
  hyphen_index_.reset();
  has_any_hyphens_ = false;
  resume_block_in_inline_in_same_flow_ = false;

  // Use 'text-indent' as the initial position. This lets tab positions to align
  // regardless of 'text-indent'.
  position_ = line_info->TextIndent();

  has_cloned_box_decorations_ = false;
  if (UNLIKELY((break_token_ && break_token_->HasClonedBoxDecorations()) ||
               cloned_box_decorations_count_))
    RecalcClonedBoxDecorations();

  ResetRewindLoopDetector();
#if DCHECK_IS_ON()
  has_considered_creating_break_token_ = false;
#endif
}

void LineBreaker::NextLine(LineInfo* line_info) {
  PrepareNextLine(line_info);

  if (break_token_ && break_token_->IsInParallelBlockFlow()) {
    const auto* block_break_token = break_token_->BlockBreakToken();
    DCHECK(block_break_token);
    const InlineItem& item = Items()[break_token_->StartItemIndex()];
    DCHECK_EQ(item.GetLayoutObject(),
              block_break_token->InputNode().GetLayoutBox());
    if (block_break_token->InputNode().IsFloating()) {
      HandleFloat(item, block_break_token, line_info);
    } else {
      // Overflowed block-in-inline, i.e. in a parallel flow.
      DCHECK_EQ(item.Type(), InlineItem::kBlockInInline);
      HandleBlockInInline(item, block_break_token, line_info);
    }

    state_ = LineBreakState::kDone;
    line_info->SetIsEmptyLine();
    return;
  }

  BreakLine(line_info);

  if (UNLIKELY(HasHyphen()))
    FinalizeHyphen(line_info->MutableResults());
  RemoveTrailingCollapsibleSpace(line_info);

  const InlineItemResults& item_results = line_info->Results();
#if DCHECK_IS_ON()
  for (const auto& result : item_results)
    result.CheckConsistency(mode_ == LineBreakerMode::kMinContent);
#endif

  // We should create a line-box when:
  //  - We have an item which needs a line box (text, etc).
  //  - A list-marker is present, and it would be the last line or last line
  //    before a forced new-line.
  //  - During min/max content sizing (to correctly determine the line width).
  //
  // TODO(kojii): There are cases where we need to PlaceItems() without creating
  // line boxes. These cases need to be reviewed.
  const bool should_create_line_box =
      ShouldCreateLineBox(item_results) ||
      (force_non_empty_if_last_line_ && line_info->IsLastLine()) ||
      mode_ != LineBreakerMode::kContent;

  if (!should_create_line_box) {
    if (To<LayoutBlockFlow>(node_.GetLayoutBox())->HasLineIfEmpty())
      line_info->SetHasLineEvenIfEmpty();
    else
      line_info->SetIsEmptyLine();
  }

  line_info->SetEndItemIndex(current_.item_index);
  DCHECK_NE(trailing_whitespace_, WhitespaceState::kUnknown);
  if (trailing_whitespace_ == WhitespaceState::kPreserved)
    line_info->SetHasTrailingSpaces();

  if (UNLIKELY(override_available_width_)) {
    // Clear the overridden available width so that `line_info` has the original
    // available width for aligning.
    override_available_width_ = LayoutUnit();
    UpdateAvailableWidth();
  }
  ComputeLineLocation(line_info);
  if (mode_ == LineBreakerMode::kContent) {
    line_info->SetBreakToken(CreateBreakToken(*line_info));
  }

#if EXPENSIVE_DCHECKS_ARE_ON()
  if (UNLIKELY(break_at_)) {
    // If `break_at_` is set, the line should break `break_at_.offset`, but due
    // to minor differences in trailing spaces, it may not match exactly. It
    // should at least be beyond `break_at_.end`.
    DCHECK_GE(line_info->End(), break_at_.end);
  }
#endif  // EXPENSIVE_DCHECKS_ARE_ON()
}

void LineBreaker::BreakLine(LineInfo* line_info) {
  DCHECK(!line_info->IsLastLine());
  const HeapVector<InlineItem>& items = Items();
  state_ = LineBreakState::kContinue;
  trailing_whitespace_ = WhitespaceState::kLeading;
  while (state_ != LineBreakState::kDone) {
    // If we reach at the end of the block, this is the last line.
    DCHECK_LE(current_.item_index, items.size());
    if (current_.item_index == items.size()) {
      // Still check overflow because the last item may have overflowed.
      if (HandleOverflowIfNeeded(line_info) &&
          current_.item_index != items.size()) {
        continue;
      }
      if (UNLIKELY(HasHyphen()))
        position_ -= RemoveHyphen(line_info->MutableResults());
      line_info->SetIsLastLine(true);
      return;
    }
    if (UNLIKELY(break_at_) && current_ >= break_at_.offset) {
      return;
    }

    // If |state_| is overflow, break at the earliest break opportunity.
    const InlineItemResults& item_results = line_info->Results();
    if (UNLIKELY(state_ == LineBreakState::kOverflow &&
                 CanBreakAfterLast(item_results))) {
      state_ = LineBreakState::kTrailing;
    }

    // Handle trailable items first. These items may not be break before.
    // They (or part of them) may also overhang the available width.
    const InlineItem& item = items[current_.item_index];
    if (item.Type() == InlineItem::kText) {
      if (item.Length())
        HandleText(item, *item.TextShapeResult(), line_info);
      else
        HandleEmptyText(item, line_info);
#if DCHECK_IS_ON()
      if (!item_results.empty())
        item_results.back().CheckConsistency(true);
#endif
      continue;
    }
    if (item.Type() == InlineItem::kOpenTag) {
      HandleOpenTag(item, line_info);
      continue;
    }
    if (item.Type() == InlineItem::kCloseTag) {
      HandleCloseTag(item, line_info);
      continue;
    }
    if (item.Type() == InlineItem::kControl) {
      HandleControlItem(item, line_info);
      continue;
    }
    if (item.Type() == InlineItem::kFloating) {
      HandleFloat(item, /* float_break_token */ nullptr, line_info);
      continue;
    }
    if (item.Type() == InlineItem::kBidiControl) {
      HandleBidiControlItem(item, line_info);
      continue;
    }
    if (item.Type() == InlineItem::kBlockInInline) {
      const NGBlockBreakToken* block_break_token =
          break_token_ ? break_token_->BlockBreakToken() : nullptr;
      HandleBlockInInline(item, block_break_token, line_info);
      continue;
    }

    // Items after this point are not trailable. If we're trailing, break before
    // any non-trailable items
    DCHECK(!IsTrailableItemType(item.Type()));
    if (state_ == LineBreakState::kTrailing) {
      DCHECK(!line_info->IsLastLine());
      return;
    }

    if (item.Type() == InlineItem::kAtomicInline) {
      HandleAtomicInline(item, line_info);
      continue;
    }
    if (UNLIKELY(item.Type() == InlineItem::kInitialLetterBox)) {
      HandleInitialLetter(item, line_info);
      continue;
    }
    if (item.Type() == InlineItem::kOutOfFlowPositioned) {
      HandleOutOfFlowPositioned(item, line_info);
    } else if (item.Length()) {
      NOTREACHED();
      // For other items with text (e.g., bidi controls), use their text to
      // determine the break opportunity.
      InlineItemResult* item_result = AddItem(item, line_info);
      item_result->can_break_after =
          break_iterator_.IsBreakable(item_result->EndOffset());
      MoveToNextOf(item);
    } else if (item.Type() == InlineItem::kListMarker) {
      InlineItemResult* item_result = AddItem(item, line_info);
      force_non_empty_if_last_line_ = true;
      DCHECK(!item_result->can_break_after);
      MoveToNextOf(item);
    } else {
      NOTREACHED();
      MoveToNextOf(item);
    }
  }
}

void LineBreaker::ComputeLineLocation(LineInfo* line_info) const {
  // Negative margins can make the position negative, but the inline size is
  // always positive or 0.
  LayoutUnit available_width = AvailableWidth();
  line_info->SetWidth(available_width,
                      position_ + cloned_box_decorations_end_size_);
  line_info->SetBfcOffset(
      {line_opportunity_.line_left_offset, line_opportunity_.bfc_block_offset});
  if (mode_ == LineBreakerMode::kContent) {
    line_info->UpdateTextAlign();
  }
}

// Atomic inlines have break opportunities before and after, even when the
// adjacent character is U+00A0 NO-BREAK SPACE character, except when sticky
// images quirk is applied.
// Note: We treat text combine as text content instead of atomic inline box[1].
// [1] https://drafts.csswg.org/css-writing-modes-3/#text-combine-layout
bool LineBreaker::CanBreakAfterAtomicInline(const InlineItem& item) const {
  DCHECK(item.Type() == InlineItem::kAtomicInline ||
         item.Type() == InlineItem::kInitialLetterBox);
  if (!auto_wrap_) {
    return false;
  }
  if (item.EndOffset() == Text().length()) {
    return true;
  }
  // We can not break before sticky images quirk was applied.
  if (item.IsImage())
    return !sticky_images_quirk_;

  if (item.IsRubyColumn()) {
    return break_iterator_.IsBreakable(item.EndOffset());
  }

  // Handles text combine
  // See "fast/writing-mode/text-combine-line-break.html".
  auto* const text_combine = MayBeTextCombine(&item);
  if (LIKELY(!text_combine))
    return true;

  // Populate |text_content| with |item| and text content after |item|.
  StringBuilder text_content;
  InlineNode(text_combine).PrepareLayoutIfNeeded();
  text_content.Append(text_combine->GetTextContent());
  const auto text_combine_end_offset = text_content.length();
  auto* const atomic_inline_item = TryGetAtomicInlineItemAfter(item);
  if (auto* next_text_combine = MayBeTextCombine(atomic_inline_item)) {
    // Note: In |LineBreakerMode::k{Min,Max}Content|, we've not laid
    // out atomic line box yet.
    InlineNode(next_text_combine).PrepareLayoutIfNeeded();
    text_content.Append(next_text_combine->GetTextContent());
  } else {
    text_content.Append(StringView(Text(), item.EndOffset(),
                                   Text().length() - item.EndOffset()));
  }

  DCHECK_EQ(Text(), break_iterator_.GetString());
  LazyLineBreakIterator break_iterator(break_iterator_,
                                       text_content.ReleaseString());
  return break_iterator.IsBreakable(text_combine_end_offset);
}

bool LineBreaker::CanBreakAfter(const InlineItem& item) const {
  DCHECK_NE(item.Type(), InlineItem::kAtomicInline);
  DCHECK(auto_wrap_);
  const bool can_break_after = break_iterator_.IsBreakable(item.EndOffset());
  if (item.Type() != InlineItem::kText) {
    DCHECK_EQ(item.Type(), InlineItem::kControl) << "We get the test case!";
    // Example: <div>12345\t\t678</div>
    //  InlineItem[0] kText "12345"
    //  InlineItem[1] kControl "\t\t"
    //  InlineItem[2] kText "678"
    // See LineBreakerTest.OverflowTab
    return can_break_after;
  }
  auto* const atomic_inline_item = TryGetAtomicInlineItemAfter(item);
  if (!atomic_inline_item)
    return can_break_after;

  if (atomic_inline_item->IsRubyColumn()) {
    return can_break_after;
  }

  // We can not break before sticky images quirk was applied.
  if (UNLIKELY(Text()[atomic_inline_item->StartOffset()] ==
               kNoBreakSpaceCharacter)) {
    // "One " <img> => We can break after "One ".
    // "One" <img> => We can not break after "One".
    // See "tables/mozilla/bugs/bug101674.html"
    DCHECK(atomic_inline_item->IsImage() && sticky_images_quirk_);
    return can_break_after;
  }

  // Handles text combine as its text contents followed by |item|.
  // See "fast/writing-mode/text-combine-line-break.html".
  auto* const text_combine = MayBeTextCombine(atomic_inline_item);
  if (LIKELY(!text_combine))
    return true;

  // Populate |text_content| with |item| and |text_combine|.
  // Following test reach here:
  //  * fast/writing-mode/text-combine-compress.html
  //  * virtual/text-antialias/international/text-combine-image-test.html
  //  * virtual/text-antialias/international/text-combine-text-transform.html
  StringBuilder text_content;
  text_content.Append(StringView(Text(), item.StartOffset(), item.Length()));
  const auto item_end_offset = text_content.length();
  // Note: In |LineBreakerMode::k{Min,Max}Content|, we've not laid out
  // atomic line box yet.
  InlineNode(text_combine).PrepareLayoutIfNeeded();
  text_content.Append(text_combine->GetTextContent());

  DCHECK_EQ(Text(), break_iterator_.GetString());
  LazyLineBreakIterator break_iterator(break_iterator_,
                                       text_content.ReleaseString());
  return break_iterator.IsBreakable(item_end_offset);
}

bool LineBreaker::MayBeAtomicInline(wtf_size_t offset) const {
  DCHECK_LT(offset, Text().length());
  const auto char_code = Text()[offset];
  if (char_code == kObjectReplacementCharacter)
    return true;
  return sticky_images_quirk_ && char_code == kNoBreakSpaceCharacter;
}

const InlineItem* LineBreaker::TryGetAtomicInlineItemAfter(
    const InlineItem& item) const {
  DCHECK(auto_wrap_);
  const String& text = Text();
  if (item.EndOffset() == text.length())
    return nullptr;
  if (!MayBeAtomicInline(item.EndOffset()))
    return nullptr;

  // This kObjectReplacementCharacter can be any objects, such as a floating or
  // an OOF object. Check if it's really an atomic inline.
  const HeapVector<InlineItem>& items = Items();
  for (const InlineItem* next_item = std::next(&item); next_item != items.end();
       ++next_item) {
    DCHECK_EQ(next_item->StartOffset(), item.EndOffset());
    if (next_item->Type() == InlineItem::kAtomicInline) {
      return next_item;
    }
    if (next_item->EndOffset() > item.EndOffset())
      return nullptr;
  }
  return nullptr;
}

void LineBreaker::HandleText(const InlineItem& item,
                             const ShapeResult& shape_result,
                             LineInfo* line_info) {
  DCHECK(item.Type() == InlineItem::kText ||
         (item.Type() == InlineItem::kControl &&
          Text()[item.StartOffset()] == kTabulationCharacter));
  DCHECK(&shape_result);
  DCHECK_EQ(auto_wrap_, ShouldAutoWrap(*item.Style()));

  // If we're trailing, only trailing spaces can be included in this line.
  if (UNLIKELY(state_ == LineBreakState::kTrailing)) {
    HandleTrailingSpaces(item, &shape_result, line_info);
    return;
  }

  // Skip leading collapsible spaces.
  // Most cases such spaces are handled as trailing spaces of the previous line,
  // but there are some cases doing so is too complex.
  if (trailing_whitespace_ == WhitespaceState::kLeading) {
    if (item.Style()->ShouldCollapseWhiteSpaces() &&
        Text()[current_.text_offset] == kSpaceCharacter) {
      // Skipping one whitespace removes all collapsible spaces because
      // collapsible spaces are collapsed to single space in
      // InlineItemBuilder.
      ++current_.text_offset;
      if (current_.text_offset == item.EndOffset()) {
        HandleEmptyText(item, line_info);
        return;
      }
    }
    // |trailing_whitespace_| will be updated as we read the text.
  }

  // Go to |HandleOverflow()| if the last item overflowed, and we're adding
  // text.
  if (state_ == LineBreakState::kContinue && !CanFitOnLine()) {
    // |HandleOverflow()| expects all trailable items are added. If this text
    // starts with trailable spaces, add them. TODO(kojii): This can be
    // optimzied further. This is necesasry only if |HandleOverflow()| does not
    // rewind, but in most cases it will rewind.
    const String& text = Text();
    if (auto_wrap_ && IsBreakableSpace(text[current_.text_offset])) {
      HandleTrailingSpaces(item, &shape_result, line_info);
      if (state_ != LineBreakState::kDone) {
        state_ = LineBreakState::kContinue;
        return;
      }
    }
    HandleOverflow(line_info);
    return;
  }

  if (UNLIKELY(HasHyphen()))
    position_ -= RemoveHyphen(line_info->MutableResults());

  InlineItemResult* item_result = nullptr;
  if (!is_svg_text_) {
    item_result = AddItem(item, line_info);
    item_result->should_create_line_box = true;
  }
  // Try to commit |pending_end_overhang_| of a prior InlineItemResult.
  // |pending_end_overhang_| doesn't work well with bidi reordering. It's
  // difficult to compute overhang after bidi reordering because it affect
  // line breaking.
  if (maybe_have_end_overhang_)
    position_ -= CommitPendingEndOverhang(line_info);

  if (auto_wrap_) {
    if (mode_ == LineBreakerMode::kMinContent &&
        HandleTextForFastMinContent(item_result, item, shape_result,
                                    line_info)) {
      return;
    }

    // Try to break inside of this text item.
    const LayoutUnit available_width = RemainingAvailableWidth();
    BreakResult break_result =
        BreakText(item_result, item, shape_result, available_width,
                  available_width, line_info);
    DCHECK(item_result->shape_result || !item_result->TextOffset().Length() ||
           (break_result == kOverflow && break_anywhere_if_overflow_ &&
            !override_break_anywhere_));
    position_ += item_result->inline_size;
    MoveToNextOf(*item_result);

    if (break_result == kSuccess) {
      DCHECK(item_result->shape_result || !item_result->TextOffset().Length());

      // If the break is at the middle of a text item, we know no trailable
      // items follow, only trailable spaces if any. This is very common that
      // shortcut to handling trailing spaces.
      if (item_result->EndOffset() < item.EndOffset())
        return HandleTrailingSpaces(item, &shape_result, line_info);

      // The break point found at the end of this text item. Continue looking
      // next items, because the next item maybe trailable, or can prohibit
      // breaking before.
      return;
    }
    if (UNLIKELY(break_result == kBreakAt)) {
      // If this break is caused by `break_at_`, only trailing spaces or
      // trailing items can follow.
      if (item_result->EndOffset() < item.EndOffset()) {
        HandleTrailingSpaces(item, &shape_result, line_info);
        return;
      }
      state_ = LineBreakState::kTrailing;
      return;
    }
    DCHECK_EQ(break_result, kOverflow);

    // Handle `overflow-wrap` if it is enabled and if this text item overflows.
    if (UNLIKELY(!item_result->shape_result)) {
      DCHECK(break_anywhere_if_overflow_ && !override_break_anywhere_);
      HandleOverflow(line_info);
      return;
    }

    // Hanging trailing spaces may resolve the overflow.
    if (item_result->has_only_trailing_spaces) {
      state_ = LineBreakState::kTrailing;
      if (item_result->item->Style()->ShouldPreserveWhiteSpaces() &&
          IsBreakableSpace(Text()[item_result->EndOffset() - 1])) {
        unsigned end_index = base::checked_cast<unsigned>(
            item_result - line_info->Results().begin());
        Rewind(end_index, line_info);
      }
      return;
    }

    // If we're seeking for the first break opportunity, update the state.
    if (UNLIKELY(state_ == LineBreakState::kOverflow)) {
      if (item_result->can_break_after)
        state_ = LineBreakState::kTrailing;
      return;
    }

    // If this is all trailable spaces, this item is trailable, and next item
    // maybe too. Don't go to |HandleOverflow()| yet.
    if (IsAllBreakableSpaces(Text(), item_result->StartOffset(),
                             item_result->EndOffset()))
      return;

    HandleOverflow(line_info);
    return;
  }

  if (is_svg_text_) {
    SplitTextIntoSegments(item, line_info);
    return;
  }

  // Add until the end of the item if !auto_wrap. In most cases, it's the whole
  // item.
  DCHECK_EQ(item_result->EndOffset(), item.EndOffset());
  if (item_result->StartOffset() == item.StartOffset()) {
    item_result->inline_size =
        shape_result.SnappedWidth().ClampNegativeToZero();
    item_result->shape_result = ShapeResultView::Create(&shape_result);
  } else {
    // <wbr> can wrap even if !auto_wrap. Spaces after that will be leading
    // spaces and thus be collapsed.
    DCHECK(trailing_whitespace_ == WhitespaceState::kLeading &&
           item_result->StartOffset() >= item.StartOffset());
    item_result->shape_result = ShapeResultView::Create(
        &shape_result, item_result->StartOffset(), item_result->EndOffset());
    item_result->inline_size =
        item_result->shape_result->SnappedWidth().ClampNegativeToZero();
  }

  DCHECK(!item_result->may_break_inside);
  DCHECK(!item_result->can_break_after);
  trailing_whitespace_ = WhitespaceState::kUnknown;
  position_ += item_result->inline_size;
  MoveToNextOf(item);
}

// In SVG <text>, we produce InlineItemResult split into segments partitioned
// by x/y/dx/dy/rotate attributes.
//
// Split in PrepareLayout() or after producing FragmentItem would need
// additional memory overhead. So we split in LineBreaker while it converts
// InlineItems to InlineItemResults.
void LineBreaker::SplitTextIntoSegments(const InlineItem& item,
                                        LineInfo* line_info) {
  DCHECK(is_svg_text_);
  DCHECK_EQ(current_.text_offset, item.StartOffset());

  const ShapeResult& shape = *item.TextShapeResult();
  if (shape.NumGlyphs() == 0 || !needs_svg_segmentation_) {
    InlineItemResult* result = AddItem(item, line_info);
    result->should_create_line_box = true;
    result->shape_result = ShapeResultView::Create(&shape);
    result->inline_size = shape.SnappedWidth();
    current_.text_offset = item.EndOffset();
    position_ += result->inline_size;
    trailing_whitespace_ = WhitespaceState::kUnknown;
    MoveToNextOf(item);
    return;
  }

  Vector<unsigned> index_list;
  index_list.reserve(shape.NumGlyphs());
  shape.ForEachGlyph(0, CollectCharIndex, &index_list);
  if (shape.IsRtl())
    index_list.Reverse();
  wtf_size_t size = index_list.size();
  unsigned glyph_start = current_.text_offset;
  for (wtf_size_t i = 0; i < size; ++i) {
#if DCHECK_IS_ON()
    // The first glyph index can be greater than StartIndex() if the leading
    // part of the string was not mapped to any glyphs.
    if (i == 0)
      DCHECK_LE(glyph_start, index_list[0]);
    else
      DCHECK_EQ(glyph_start, index_list[i]);
#endif
    unsigned glyph_end = i + 1 < size ? index_list[i + 1] : shape.EndIndex();
    StringView text_view(Text());
    bool should_split = i == size - 1;
    for (; glyph_start < glyph_end;
         glyph_start = text_view.NextCodePointOffset(glyph_start)) {
      ++svg_addressable_offset_;
      should_split = should_split || ShouldCreateNewSvgSegment();
    }
    if (!should_split)
      continue;
    InlineItemResult* result = AddItem(item, glyph_end, line_info);
    result->should_create_line_box = true;
    auto shape_result_view =
        ShapeResultView::Create(&shape, current_.text_offset, glyph_end);
    // For general CSS text, we apply SnappedWidth().ClampNegativeToZero().
    // However we need to remove ClampNegativeToZero() for SVG <text> in order
    // to get similar character positioning.
    //
    // For general CSS text, a negative word-spacing value decreases
    // inline_size of an InlineItemResult consisting of multiple characters,
    // and the inline_size rarely becomes negative.  However, for SVG <text>,
    // it decreases inline_size of an InlineItemResult consisting of only a
    // space character, and the inline_size becomes negative easily.
    //
    // See svg/W3C-SVG-1.1/text-spacing-01-b.svg.
    result->inline_size = shape_result_view->SnappedWidth();
    result->shape_result = std::move(shape_result_view);
    current_.text_offset = glyph_end;
    position_ += result->inline_size;
  }
  trailing_whitespace_ = WhitespaceState::kUnknown;
  MoveToNextOf(item);
}

bool LineBreaker::ShouldCreateNewSvgSegment() const {
  DCHECK(is_svg_text_);
  for (const auto& range : node_.SvgTextPathRangeList()) {
    if (range.start_index <= svg_addressable_offset_ &&
        svg_addressable_offset_ <= range.end_index)
      return true;
  }
  for (const auto& range : node_.SvgTextLengthRangeList()) {
    if (To<SVGTextContentElement>(range.layout_object->GetNode())
            ->lengthAdjust()
            ->CurrentEnumValue() == kSVGLengthAdjustSpacingAndGlyphs)
      continue;
    if (range.start_index <= svg_addressable_offset_ &&
        svg_addressable_offset_ <= range.end_index)
      return true;
  }
  const SvgCharacterData& char_data =
      svg_resolved_iterator_->AdvanceTo(svg_addressable_offset_);
  return char_data.HasRotate() || char_data.HasX() || char_data.HasY() ||
         char_data.HasDx() || char_data.HasDy();
}

LineBreaker::BreakResult LineBreaker::BreakText(
    InlineItemResult* item_result,
    const InlineItem& item,
    const ShapeResult& item_shape_result,
    LayoutUnit available_width,
    LayoutUnit available_width_with_hyphens,
    LineInfo* line_info) {
  DCHECK(item.Type() == InlineItem::kText ||
         (item.Type() == InlineItem::kControl &&
          Text()[item.StartOffset()] == kTabulationCharacter));
  DCHECK(&item_shape_result);
  item.AssertOffset(item_result->StartOffset());

  // The hyphenation state should be cleared before the entry. This function
  // may reset it, but this function cannot determine whether it should update
  // |position_| or not.
  DCHECK(!HasHyphen());

  DCHECK_EQ(item_shape_result.StartIndex(), item.StartOffset());
  DCHECK_EQ(item_shape_result.EndIndex(), item.EndOffset());
  class ShapingLineBreakerImpl : public ShapingLineBreaker {
    STACK_ALLOCATED();

   public:
    ShapingLineBreakerImpl(LineBreaker* line_breaker,
                           const InlineItem* item,
                           scoped_refptr<const ShapeResult> result)
        : ShapingLineBreaker(std::move(result),
                             &line_breaker->break_iterator_,
                             line_breaker->hyphenation_),
          line_breaker_(line_breaker),
          item_(item) {}

   protected:
    scoped_refptr<ShapeResult> Shape(unsigned start,
                                     unsigned end,
                                     ShapeOptions options) final {
      return line_breaker_->ShapeText(*item_, start, end, options);
    }

   private:
    LineBreaker* line_breaker_;
    const InlineItem* item_;

  } breaker(this, &item, &item_shape_result);

  const ComputedStyle& style = *item.Style();
  breaker.SetTextSpacingTrim(style.GetFontDescription().GetTextSpacingTrim());
  breaker.SetLineStart(line_info->StartOffset());

  // Reshaping between the last character and trailing spaces is needed only
  // when we need accurate end position, because kerning between trailing spaces
  // is not visible.
  if (!NeedsAccurateEndPosition(*line_info, item))
    breaker.SetDontReshapeEndIfAtSpace();

  if (UNLIKELY(break_at_)) {
    if (BreakTextAt(item_result, item, breaker, line_info)) {
      return kBreakAt;
    }
    return kSuccess;
  }

  // Use kNoResultIfOverflow if 'break-word' and we're trying to break normally
  // because if this item overflows, we will rewind and break line again. The
  // overflowing ShapeResult is not needed.
  if (break_anywhere_if_overflow_ && !override_break_anywhere_)
    breaker.SetNoResultIfOverflow();

#if DCHECK_IS_ON()
  unsigned try_count = 0;
#endif
  LayoutUnit inline_size;
  ShapingLineBreaker::Result result;
  while (true) {
#if DCHECK_IS_ON()
    ++try_count;
    DCHECK_LE(try_count, 2u);
#endif
    scoped_refptr<const ShapeResultView> shape_result =
        breaker.ShapeLine(item_result->StartOffset(),
                          available_width.ClampNegativeToZero(), &result);

    // If this item overflows and 'break-word' is set, this line will be
    // rewinded. Making this item long enough to overflow is enough.
    if (!shape_result) {
      DCHECK(breaker.NoResultIfOverflow());
      item_result->inline_size = available_width_with_hyphens + 1;
      item_result->text_offset.end = item.EndOffset();
      item_result->text_offset.AssertNotEmpty();
      return kOverflow;
    }
    DCHECK_EQ(shape_result->NumCharacters(),
              result.break_offset - item_result->StartOffset());
    // It is critical to move the offset forward, or LineBreaker may keep
    // adding InlineItemResult until all the memory is consumed.
    CHECK_GT(result.break_offset, item_result->StartOffset());

    inline_size = shape_result->SnappedWidth().ClampNegativeToZero();
    item_result->inline_size = inline_size;
    if (UNLIKELY(result.is_hyphenated)) {
      InlineItemResults* item_results = line_info->MutableResults();
      const LayoutUnit hyphen_inline_size =
          AddHyphen(item_results, item_result);
      // If the hyphen overflows, retry with the reduced available width.
      if (!result.is_overflow && inline_size <= available_width) {
        const LayoutUnit space_for_hyphen =
            available_width_with_hyphens - inline_size;
        if (space_for_hyphen >= 0 && hyphen_inline_size > space_for_hyphen) {
          available_width -= hyphen_inline_size;
          RemoveHyphen(item_results);
          continue;
        }
      }
      inline_size = item_result->inline_size;
    }
    item_result->text_offset.end = result.break_offset;
    item_result->text_offset.AssertNotEmpty();
    item_result->has_only_trailing_spaces = result.has_trailing_spaces;
    item_result->shape_result = std::move(shape_result);
    break;
  }

  // * If width <= available_width:
  //   * If offset < item.EndOffset(): the break opportunity to fit is found.
  //   * If offset == item.EndOffset(): the break opportunity at the end fits,
  //     or the first break opportunity is beyond the end.
  //     There may be room for more characters.
  // * If width > available_width: The first break opportunity does not fit.
  //   offset is the first break opportunity, either inside, at the end, or
  //   beyond the end.
  if (item_result->EndOffset() < item.EndOffset()) {
    item_result->can_break_after = true;

    if (UNLIKELY(break_iterator_.BreakType() ==
                 LineBreakType::kBreakCharacter)) {
      trailing_whitespace_ = WhitespaceState::kUnknown;
    } else {
      trailing_whitespace_ = WhitespaceState::kNone;
    }
  } else {
    DCHECK_EQ(item_result->EndOffset(), item.EndOffset());
    item_result->can_break_after = CanBreakAfter(item);
    trailing_whitespace_ = WhitespaceState::kUnknown;
  }

  // This result is not breakable any further if overflow. This information is
  // useful to optimize |HandleOverflow()|.
  item_result->may_break_inside = !result.is_overflow;

  // TODO(crbug.com/1003742): We should use |result.is_overflow| here. For now,
  // use |inline_size| because some tests rely on this behavior.
  return inline_size <= available_width_with_hyphens ? kSuccess : kOverflow;
}

bool LineBreaker::BreakTextAt(InlineItemResult* item_result,
                              const InlineItem& item,
                              ShapingLineBreaker& breaker,
                              LineInfo* line_info) {
  DCHECK(break_at_);
  DCHECK_LE(current_.text_offset, break_at_.end.text_offset);
  DCHECK_LE(current_.item_index, break_at_.offset.item_index);
  const bool should_break = current_.item_index >= break_at_.end.item_index;
  if (should_break) {
    DCHECK_LE(break_at_.end.text_offset, item_result->text_offset.end);
    item_result->text_offset.end = break_at_.end.text_offset;
    item_result->text_offset.AssertValid();
  } else {
    DCHECK_GE(break_at_.end.text_offset, item_result->text_offset.end);
  }
  if (item_result->Length()) {
    scoped_refptr<const ShapeResultView> shape_result = breaker.ShapeLineAt(
        item_result->StartOffset(), item_result->EndOffset());
    item_result->inline_size =
        shape_result->SnappedWidth().ClampNegativeToZero();
    item_result->shape_result = std::move(shape_result);
    if (break_at_.is_hyphenated) {
      AddHyphen(line_info->MutableResults(), item_result);
    }
  } else {
    DCHECK_EQ(item_result->inline_size, LayoutUnit());
    DCHECK(!break_at_.is_hyphenated);
  }
  item_result->can_break_after = true;
  trailing_whitespace_ = WhitespaceState::kNone;
  return should_break;
}

// Breaks the text item at the previous break opportunity from
// |item_result->text_offset.end|. Returns false if there were no previous break
// opportunities.
bool LineBreaker::BreakTextAtPreviousBreakOpportunity(
    InlineItemResult* item_result) {
  DCHECK(item_result->item);
  DCHECK(item_result->may_break_inside);
  const InlineItem& item = *item_result->item;
  DCHECK_EQ(item.Type(), InlineItem::kText);
  DCHECK(item.Style() && item.Style()->ShouldWrapLine());
  DCHECK(!is_text_combine_);

  // TODO(jfernandez): Should we use the non-hangable-run-end instead ?
  unsigned break_opportunity = break_iterator_.PreviousBreakOpportunity(
      item_result->EndOffset() - 1, item_result->StartOffset());
  if (break_opportunity <= item_result->StartOffset())
    return false;
  item_result->text_offset.end = break_opportunity;
  item_result->text_offset.AssertNotEmpty();
  item_result->shape_result = ShapeResultView::Create(
      item.TextShapeResult(), item_result->StartOffset(),
      item_result->EndOffset());
  item_result->inline_size =
      item_result->shape_result->SnappedWidth().ClampNegativeToZero();
  item_result->can_break_after = true;

  if (trailing_collapsible_space_.has_value() &&
      trailing_collapsible_space_->item_result == item_result) {
    trailing_collapsible_space_.reset();
  }

  return true;
}

// This function handles text item for min-content. The specialized logic is
// because min-content is very expensive by breaking at every break opportunity
// and producing as many lines as the number of break opportunities.
//
// This function breaks the text in InlineItem at every break opportunity,
// computes the maximum width of all words, and creates one InlineItemResult
// that has the maximum width. For example, for a text item of "1 2 34 5 6",
// only the width of "34" matters for min-content.
//
// The first word and the last word, "1" and "6" in the example above, are
// handled in normal |HandleText()| because they may form a word with the
// previous/next item.
bool LineBreaker::HandleTextForFastMinContent(InlineItemResult* item_result,
                                              const InlineItem& item,
                                              const ShapeResult& shape_result,
                                              LineInfo* line_info) {
  DCHECK_EQ(mode_, LineBreakerMode::kMinContent);
  DCHECK(auto_wrap_);
  DCHECK(item.Type() == InlineItem::kText ||
         (item.Type() == InlineItem::kControl &&
          Text()[item.StartOffset()] == kTabulationCharacter));
  DCHECK(&shape_result);

  // If this is the first part of the text, it may form a word with the previous
  // item. Fallback to |HandleText()|.
  unsigned start_offset = item_result->StartOffset();
  DCHECK_LT(start_offset, item.EndOffset());
  if (start_offset != line_info->StartOffset() &&
      start_offset == item.StartOffset())
    return false;
  if (UNLIKELY(line_info->TextIndent())) {
    // If this line has the `text-indent`, following lines will have different
    // indentation. Compute this line as a separate line by falling back to
    // |HandleText()|.
    return false;
  }
  // If this is the last part of the text, it may form a word with the next
  // item. Fallback to |HandleText()|.
  if (fast_min_content_item_ == &item)
    return false;

  absl::optional<LineBreakType> saved_line_break_type;
  if (break_anywhere_if_overflow_ && !override_break_anywhere_) {
    saved_line_break_type = break_iterator_.BreakType();
    break_iterator_.SetBreakType(LineBreakType::kBreakCharacter);
  }

  // Break the text at every break opportunity and measure each word.
  DCHECK_EQ(shape_result.StartIndex(), item.StartOffset());
  DCHECK_GE(start_offset, shape_result.StartIndex());
  shape_result.EnsurePositionData();
  const String& text = Text();
  const bool should_break_spaces = item.Style()->ShouldBreakSpaces();
  float min_width = 0;
  unsigned last_end_offset = 0;
  unsigned end_offset = start_offset + 1;
  absl::optional<LayoutUnit> hyphen_inline_size;
  while (start_offset < item.EndOffset()) {
    end_offset =
        break_iterator_.NextBreakOpportunity(end_offset, item.EndOffset());

    unsigned non_hangable_run_end = end_offset;
    if (!should_break_spaces) {
      while (non_hangable_run_end > start_offset &&
             IsBreakableSpace(text[non_hangable_run_end - 1])) {
        --non_hangable_run_end;
      }
    }

    if (non_hangable_run_end >= item.EndOffset())
      break;

    // |word_len| may be zero if |start_offset| is at a breakable space.
    CHECK_GE(non_hangable_run_end, start_offset);
    if (wtf_size_t word_len = non_hangable_run_end - start_offset) {
      // Ignore soft-hyphen opportunities if `hyphens: none`.
      bool has_hyphen = text[non_hangable_run_end - 1] == kSoftHyphenCharacter;
      if (UNLIKELY(hyphenation_)) {
        // When 'hyphens: auto', compute all hyphenation opportunities.
        if (!hyphen_inline_size) {
          if (!item_result->hyphen) {
            item_result->ShapeHyphen();
          }
          hyphen_inline_size = item_result->hyphen.InlineSize();
        }
        const StringView word(text, start_offset, word_len);
        Vector<wtf_size_t, 8> locations = hyphenation_->HyphenLocations(word);
        // |locations| is a list of hyphenation points in the descending order.
        // Append 0 to process all parts the same way.
        DCHECK(std::is_sorted(locations.rbegin(), locations.rend()));
        DCHECK(!locations.Contains(0u));
        DCHECK(!locations.Contains(word_len));
        locations.push_back(0);
        LayoutUnit max_part_width;
        for (const wtf_size_t location : locations) {
          LayoutUnit part_width = LayoutUnit::FromFloatCeil(ComputeWordWidth(
              shape_result, start_offset + location, start_offset + word_len));
          if (has_hyphen)
            part_width += *hyphen_inline_size;
          max_part_width = std::max(part_width, max_part_width);
          word_len = location;
          has_hyphen = true;
        }
        min_width = std::max(max_part_width.ToFloat(), min_width);
      } else {
        float word_width =
            ComputeWordWidth(shape_result, start_offset, non_hangable_run_end);

        // Append hyphen-width to `word_width` if the word is hyphenated.
        if (has_hyphen) {
          if (!hyphen_inline_size) {
            if (!item_result->hyphen) {
              item_result->ShapeHyphen();
            }
            hyphen_inline_size = item_result->hyphen.InlineSize();
          }
          word_width =
              (LayoutUnit::FromFloatCeil(word_width) + *hyphen_inline_size)
                  .ToFloat();
        }

        min_width = std::max(word_width, min_width);
      }
    }

    last_end_offset = non_hangable_run_end;
    start_offset = end_offset;
    ++end_offset;
  }

  if (saved_line_break_type.has_value())
    break_iterator_.SetBreakType(*saved_line_break_type);

  // If there was only one break opportunity in this item, it may form a word
  // with previous and/or next item. Fallback to |HandleText()|.
  if (!last_end_offset)
    return false;

  // Create an InlineItemResult that has the max of widths of all words.
  item_result->text_offset.end =
      std::max(last_end_offset, item_result->text_offset.start + 1);
  item_result->text_offset.AssertNotEmpty();
  item_result->inline_size = LayoutUnit::FromFloatCeil(min_width);
  item_result->can_break_after = true;

  trailing_whitespace_ = WhitespaceState::kUnknown;
  position_ += item_result->inline_size;
  state_ = LineBreakState::kTrailing;
  fast_min_content_item_ = &item;
  MoveToNextOf(*item_result);
  return true;
}

void LineBreaker::HandleEmptyText(const InlineItem& item, LineInfo* line_info) {
  // Add an empty `InlineItemResult` for empty or fully collapsed text. They
  // aren't necessary for line breaking/layout purposes, but callsites may need
  // to see all `InlineItem` by iterating `InlineItemResult`. For example,
  // `CreateLine` needs to `ClearNeedsLayout` for all `LayoutObject` including
  // empty or fully collapsed text.
  AddEmptyItem(item, line_info);
  MoveToNextOf(item);
}

// Re-shape the specified range of |InlineItem|.
scoped_refptr<ShapeResult> LineBreaker::ShapeText(const InlineItem& item,
                                                  unsigned start,
                                                  unsigned end,
                                                  ShapeOptions options) {
  scoped_refptr<ShapeResult> shape_result;
  if (!items_data_.segments) {
    RunSegmenter::RunSegmenterRange segment_range =
        InlineItemSegment::UnpackSegmentData(start, end, item.SegmentData());
    shape_result = shaper_.Shape(&item.Style()->GetFont(), item.Direction(),
                                 start, end, segment_range, options);
  } else {
    shape_result = items_data_.segments->ShapeText(
        &shaper_, &item.Style()->GetFont(), item.Direction(), start, end,
        base::checked_cast<unsigned>(&item - items_data_.items.begin()),
        options);
  }
  if (UNLIKELY(spacing_.HasSpacing()))
    shape_result->ApplySpacing(spacing_);
  return shape_result;
}

void LineBreaker::AppendCandidates(const InlineItemResult& item_result,
                                   const LineInfo& line_info,
                                   LineBreakCandidateContext& context) {
  DCHECK(item_result.item);
  const InlineItem& item = *item_result.item;
  const wtf_size_t item_index = item_result.item_index;
  DCHECK(context.GetState() == LineBreakCandidateContext::kBreak ||
         !context.Candidates().empty());

  DCHECK_EQ(item.Type(), InlineItem::kText);
  if (!item.Length()) {
    // Fully collapsed spaces don't have break opportunities.
    context.AppendTrailingSpaces(
        item_result.can_break_after ? LineBreakCandidateContext::kBreak
                                    : context.GetState(),
        {item_result.item_index, item.EndOffset()}, context.Position());
    context.SetLast(&item, item.EndOffset());
    return;
  }

  DCHECK(item.TextShapeResult());
  struct ShapeResultWrapper {
    explicit ShapeResultWrapper(const ShapeResult* shape_result)
        : shape_result(shape_result),
          shape_result_start_index(shape_result->StartIndex()),
          is_ltr(shape_result->IsLtr()) {
      shape_result->EnsurePositionData();
    }

    bool IsLtr() const { return is_ltr; }

    // The returned position is in the external coordinate system set by
    // `SetBasePosition`, not the internal one of the `ShapeResult`.
    float PositionForOffset(unsigned offset) const {
      DCHECK_GE(offset, shape_result_start_index);
      const float position = shape_result->CachedPositionForOffset(
          offset - shape_result_start_index);
      return IsLtr() ? base_position + position : base_position - position;
    }

    // Adjusts the internal coordinate system of the `ShapeResult` to the
    // specified one.
    void SetBasePosition(wtf_size_t offset, float adjusted) {
      DCHECK_GE(offset, shape_result_start_index);
      const float position = shape_result->CachedPositionForOffset(
          offset - shape_result_start_index);
      base_position = IsLtr() ? adjusted - position : adjusted + position;
      DCHECK_EQ(adjusted, PositionForOffset(offset));
    }

    unsigned PreviousSafeToBreakOffset(unsigned offset) const {
      // Unlike `PositionForOffset`, `PreviousSafeToBreakOffset` takes the
      // "external" offset that takes care of `StartIndex()`.
      return shape_result->CachedPreviousSafeToBreakOffset(offset);
    }

    const ShapeResult* const shape_result;
    const wtf_size_t shape_result_start_index;
    float base_position = .0f;
    const bool is_ltr;
  } shape_result(item.TextShapeResult());
  const String& text_content = Text();

  // Extend the end offset to the end of the item or the end of this line,
  // whichever earlier. This is not only for performance but also to include
  // trailing spaces that may be removed by the line breaker.
  TextOffsetRange offset = item_result.TextOffset();
  offset.end = std::max(offset.end,
                        std::min(item.EndOffset(), line_info.EndTextOffset()));

  // Extend the start offset to `context.last_end_offset`. Trailing spaces may
  // be skipped, or leading spaces may be already handled.
  if (context.LastItem()) {
    DCHECK_GE(context.LastEndOffset(), item.StartOffset());
    if (context.LastEndOffset() >= offset.end) {
      return;  // Return if all characters were already handled.
    }
    offset.start = context.LastEndOffset();
    offset.AssertNotEmpty();
    shape_result.SetBasePosition(offset.start, context.Position());

    // Handle leading/trailing spaces if they were skipped.
    if (IsBreakableSpace(text_content[offset.start])) {
      DCHECK_GE(offset.start, item.StartOffset());
      do {
        ++offset.start;
      } while (offset.start < offset.end &&
               IsBreakableSpace(text_content[offset.start]));
      const float end_position = shape_result.PositionForOffset(offset.start);
      if (!offset.Length()) {
        context.AppendTrailingSpaces(item_result.can_break_after
                                         ? LineBreakCandidateContext::kBreak
                                         : LineBreakCandidateContext::kMidWord,
                                     {item_index, offset.start}, end_position);
        context.SetLast(&item, offset.end);
        return;
      }
      context.AppendTrailingSpaces(auto_wrap_
                                       ? LineBreakCandidateContext::kBreak
                                       : LineBreakCandidateContext::kMidWord,
                                   {item_index, offset.start}, end_position);
    }
  } else {
    shape_result.SetBasePosition(offset.start, context.Position());
  }
  offset.AssertNotEmpty();
  DCHECK_GE(offset.start, item.StartOffset());
  DCHECK_GE(offset.start, context.LastEndOffset());
  DCHECK_LE(offset.end, item.EndOffset());
  context.SetLast(&item, offset.end);

  // Setup the style and its derived fields for this `item`.
  if (offset.start < break_iterator_.StartOffset()) {
    break_iterator_.SetStartOffset(offset.start);
  }
  DCHECK(item.Style());
  SetCurrentStyle(*item.Style());

  // Find all break opportunities in `item_result`.
  absl::optional<LayoutUnit> hyphen_advance_cache;
  for (;;) {
    // Compute the offset of the next break opportunity.
    wtf_size_t next_offset;
    if (auto_wrap_) {
      const wtf_size_t len = std::min(offset.end + 1, text_content.length());
      next_offset = break_iterator_.NextBreakOpportunity(offset.start + 1, len);
    } else {
      next_offset = offset.end + 1;
    }
    if (next_offset > offset.end && item_result.can_break_after) {
      // If `can_break_after`, honor it over `next_offset`. CSS can allow the
      // break at the end. E.g., fast/inline/line-break-atomic-inline.html
      next_offset = offset.end;
    }

    // Compute the position of the break opportunity and the end of the word.
    wtf_size_t end_offset;
    float next_position;
    float end_position;
    LineBreakCandidateContext::State next_state =
        LineBreakCandidateContext::kBreak;
    float penalty = 0;
    bool is_hyphenated = false;
    if (next_offset > offset.end) {
      // If the next break opportunity is beyond this item, stop at the end of
      // this item and set `is_middle_word`.
      end_offset = next_offset = offset.end;
      end_position = next_position =
          shape_result.PositionForOffset(next_offset);
      next_state = LineBreakCandidateContext::kMidWord;
    } else {
      if (next_offset == offset.end && !item_result.can_break_after) {
        // Can't break at `next_offset` by higher level protocols.
        // E.g., `<span>1 </span>2`.
        next_state = LineBreakCandidateContext::kMidWord;
      }
      next_position = shape_result.PositionForOffset(next_offset);

      // Exclude trailing spaces if any.
      end_offset = next_offset;
      DCHECK_GT(end_offset, offset.start);
      UChar last_ch = text_content[end_offset - 1];
      while (IsBreakableSpace(last_ch)) {
        --end_offset;
        if (end_offset == offset.start) {
          last_ch = 0;
          break;
        }
        last_ch = text_content[end_offset - 1];
      }
      DCHECK_LE(end_offset, offset.end);

      if (UNLIKELY(hyphenation_)) {
        const LayoutUnit hyphen_advance =
            HyphenAdvance(*current_style_, shape_result.IsLtr(),
                          item_result.hyphen, hyphen_advance_cache);
        DCHECK_GT(end_offset, offset.start);
        const wtf_size_t word_len = end_offset - offset.start;
        const StringView word(text_content, offset.start, word_len);
        Vector<wtf_size_t, 8> locations = hyphenation_->HyphenLocations(word);
        // |locations| is a list of hyphenation points in the descending order.
#if EXPENSIVE_DCHECKS_ARE_ON()
        DCHECK(!locations.Contains(0u));
        DCHECK(!locations.Contains(word_len));
        DCHECK(std::is_sorted(locations.rbegin(), locations.rend()));
#endif  // EXPENSIVE_DCHECKS_ARE_ON()
        const float hyphen_penalty = context.HyphenPenalty();
        InlineItemTextIndex hyphen_offset = {item_index, 0};
        for (const wtf_size_t location : base::Reversed(locations)) {
          hyphen_offset.text_offset = offset.start + location;
          const float position =
              shape_result.PositionForOffset(hyphen_offset.text_offset);
          context.Append(LineBreakCandidateContext::kBreak, hyphen_offset,
                         hyphen_offset, position, position + hyphen_advance,
                         hyphen_penalty,
                         /*is_hyphenated*/ true);
        }
      }

      // Compute the end position of this word, excluding trailing spaces.
      wtf_size_t end_safe_offset;
      switch (next_state) {
        case LineBreakCandidateContext::kBreak:
          end_safe_offset = shape_result.PreviousSafeToBreakOffset(end_offset);
          if (end_safe_offset < offset.start) {
            DCHECK_EQ(context.Candidates().back().offset.text_offset,
                      offset.start);
            end_safe_offset = offset.start;
          }
          break;
        case LineBreakCandidateContext::kMidWord:
          end_safe_offset = end_offset;
          break;
      }
      if (end_safe_offset == end_offset) {
        if (end_offset == next_offset) {
          end_position = next_position;
        } else {
          end_position = shape_result.PositionForOffset(end_offset);
        }
      } else {
        DCHECK_LT(end_safe_offset, end_offset);
        end_position = shape_result.PositionForOffset(end_safe_offset);
        scoped_refptr<ShapeResult> end_shape_result =
            ShapeText(item, end_safe_offset, end_offset);
        end_position += end_shape_result->Width();
      }

      DCHECK(!is_hyphenated);
      if (end_offset == item_result.EndOffset()) {
        is_hyphenated = item_result.is_hyphenated;
      } else if (UNLIKELY(last_ch == kSoftHyphenCharacter) &&
                 next_state == LineBreakCandidateContext::kBreak) {
        is_hyphenated = true;
      }
      if (is_hyphenated) {
        end_position += HyphenAdvance(*current_style_, shape_result.IsLtr(),
                                      item_result.hyphen, hyphen_advance_cache);
        penalty = context.HyphenPenalty();
      }
    }

    context.Append(next_state, {item_index, next_offset},
                   {item_index, end_offset}, next_position, end_position,
                   penalty, is_hyphenated);
    if (next_offset >= offset.end) {
      break;
    }
    offset.start = next_offset;
  }
}

bool LineBreaker::CanBreakInside(const LineInfo& line_info) {
  const InlineItemResults& item_results = line_info.Results();
  for (const InlineItemResult& item_result :
       base::make_span(item_results.begin(), item_results.size() - 1)) {
    if (item_result.can_break_after) {
      return true;
    }
  }
  for (const InlineItemResult& item_result : item_results) {
    DCHECK(item_result.item);
    const InlineItem& item = *item_result.item;
    if (item.Type() == InlineItem::kText) {
      if (item_result.may_break_inside && CanBreakInside(item_result)) {
        return true;
      }
    }
  }
  return false;
}

bool LineBreaker::CanBreakInside(const InlineItemResult& item_result) {
  DCHECK(item_result.may_break_inside);
  DCHECK(item_result.item);
  const InlineItem& item = *item_result.item;
  DCHECK_EQ(item.Type(), InlineItem::kText);
  DCHECK(item.Style());
  SetCurrentStyle(*item.Style());
  if (!auto_wrap_) {
    return false;
  }
  const TextOffsetRange& offset = item_result.TextOffset();
  if (offset.start < break_iterator_.StartOffset()) {
    break_iterator_.SetStartOffset(offset.start);
  }
  const wtf_size_t next_offset =
      break_iterator_.NextBreakOpportunity(offset.start + 1);
  return next_offset < offset.end;
}

// Compute a new ShapeResult for the specified end offset.
// The end is re-shaped if it is not safe-to-break.
scoped_refptr<ShapeResultView> LineBreaker::TruncateLineEndResult(
    const LineInfo& line_info,
    const InlineItemResult& item_result,
    unsigned end_offset) {
  DCHECK(item_result.item);
  const InlineItem& item = *item_result.item;

  // Check given offsets require to truncate |item_result.shape_result|.
  const unsigned start_offset = item_result.StartOffset();
  const ShapeResultView* source_result = item_result.shape_result.get();
  DCHECK(source_result);
  DCHECK_GE(start_offset, source_result->StartIndex());
  DCHECK_LE(end_offset, source_result->EndIndex());
  DCHECK(start_offset > source_result->StartIndex() ||
         end_offset < source_result->EndIndex());

  if (!NeedsAccurateEndPosition(line_info, item)) {
    return ShapeResultView::Create(source_result, start_offset, end_offset);
  }

  unsigned last_safe = source_result->PreviousSafeToBreakOffset(end_offset);
  DCHECK_LE(last_safe, end_offset);
  // TODO(abotella): Shouldn't last_safe <= start_offset trigger a reshaping?
  if (last_safe == end_offset || last_safe <= start_offset) {
    return ShapeResultView::Create(source_result, start_offset, end_offset);
  }

  scoped_refptr<ShapeResult> end_result =
      ShapeText(item, std::max(last_safe, start_offset), end_offset);
  DCHECK_EQ(end_result->Direction(), source_result->Direction());
  ShapeResultView::Segment segments[2];
  segments[0] = {source_result, start_offset, last_safe};
  segments[1] = {end_result.get(), 0, end_offset};
  return ShapeResultView::Create(segments);
}

// Update |ShapeResult| in |item_result| to match to its |start_offset| and
// |end_offset|. The end is re-shaped if it is not safe-to-break.
void LineBreaker::UpdateShapeResult(const LineInfo& line_info,
                                    InlineItemResult* item_result) {
  DCHECK(item_result);
  item_result->shape_result =
      TruncateLineEndResult(line_info, *item_result, item_result->EndOffset());
  DCHECK(item_result->shape_result);
  item_result->inline_size = item_result->shape_result->SnappedWidth();
}

inline void LineBreaker::HandleTrailingSpaces(const InlineItem& item,
                                              LineInfo* line_info) {
  const ShapeResult* shape_result = item.TextShapeResult();
  // Call |HandleTrailingSpaces| even if |item| does not have |ShapeResult|, so
  // that we skip spaces.
  HandleTrailingSpaces(item, shape_result, line_info);
}

void LineBreaker::HandleTrailingSpaces(const InlineItem& item,
                                       const ShapeResult* shape_result,
                                       LineInfo* line_info) {
  DCHECK(item.Type() == InlineItem::kText ||
         (item.Type() == InlineItem::kControl &&
          Text()[item.StartOffset()] == kTabulationCharacter));
  bool is_control_tab = item.Type() == InlineItem::kControl &&
                        Text()[item.StartOffset()] == kTabulationCharacter;
  DCHECK(item.Type() == InlineItem::kText || is_control_tab);
  DCHECK_GE(current_.text_offset, item.StartOffset());
  DCHECK_LT(current_.text_offset, item.EndOffset());
  const String& text = Text();
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();

  if (!auto_wrap_) {
    state_ = LineBreakState::kDone;
    return;
  }
  DCHECK(!is_text_combine_);

  if (style.ShouldCollapseWhiteSpaces() &&
      !Character::IsOtherSpaceSeparator(text[current_.text_offset])) {
    if (text[current_.text_offset] != kSpaceCharacter) {
      if (current_.text_offset > 0 &&
          IsBreakableSpace(text[current_.text_offset - 1])) {
        trailing_whitespace_ = WhitespaceState::kCollapsible;
      }
      state_ = LineBreakState::kDone;
      return;
    }

    // Skipping one whitespace removes all collapsible spaces because
    // collapsible spaces are collapsed to single space in InlineItemBuilder.
    current_.text_offset++;
    trailing_whitespace_ = WhitespaceState::kCollapsed;

    // Make the last item breakable after, even if it was nowrap.
    InlineItemResults* item_results = line_info->MutableResults();
    DCHECK(!item_results->empty());
    item_results->back().can_break_after = true;
  } else if (!style.ShouldBreakSpaces()) {
    // Find the end of the run of space characters in this item.
    // Other white space characters (e.g., tab) are not included in this item.
    DCHECK(style.ShouldBreakOnlyAfterWhiteSpace() ||
           Character::IsOtherSpaceSeparator(text[current_.text_offset]));
    unsigned end = current_.text_offset;
    while (end < item.EndOffset() &&
           IsBreakableSpaceOrOtherSeparator(text[end]))
      end++;
    if (end == current_.text_offset) {
      if (IsBreakableSpaceOrOtherSeparator(text[end - 1]))
        trailing_whitespace_ = WhitespaceState::kPreserved;
      state_ = LineBreakState::kDone;
      return;
    }

    // TODO (jfernandez): Could we just modify the last ItemResult
    // instead of creating a new one ?
    // Probably we can (koji). We would need to review usage of these
    // item results, and change them to use "non_hangable_run_end"
    // instead.
    DCHECK(shape_result);
    InlineItemResult* item_result = AddItem(item, end, line_info);
    item_result->should_create_line_box = true;
    item_result->has_only_trailing_spaces = true;
    item_result->shape_result = ShapeResultView::Create(shape_result);
    if (item_result->StartOffset() == item.StartOffset() &&
        item_result->EndOffset() == item.EndOffset()) {
      item_result->inline_size =
          item_result->shape_result && mode_ != LineBreakerMode::kMinContent
              ? item_result->shape_result->SnappedWidth()
              : LayoutUnit();
    } else {
      UpdateShapeResult(*line_info, item_result);
      if (mode_ == LineBreakerMode::kMinContent) {
        item_result->inline_size = LayoutUnit();
      }
    }
    position_ += item_result->inline_size;
    item_result->can_break_after =
        end < text.length() && !IsBreakableSpaceOrOtherSeparator(text[end]);
    current_.text_offset = end;
    trailing_whitespace_ = WhitespaceState::kPreserved;
  }

  // If non-space characters follow, the line is done.
  // Otherwise keep checking next items for the break point.
  DCHECK_LE(current_.text_offset, item.EndOffset());
  if (current_.text_offset < item.EndOffset()) {
    state_ = LineBreakState::kDone;
    return;
  }
  DCHECK_EQ(current_.text_offset, item.EndOffset());
  const InlineItemResults& item_results = line_info->Results();
  if (item_results.empty() || item_results.back().item != &item) {
    // If at the end of `item` but the item hasn't been added to `line_info`,
    // add an empty text item. See `HandleEmptyText`.
    AddEmptyItem(item, line_info);
  }
  current_.item_index++;
  state_ = LineBreakState::kTrailing;
}

void LineBreaker::RewindTrailingOpenTags(LineInfo* line_info) {
  // Remove trailing open tags. Open tags are included as trailable items
  // because they are ambiguous. When the line ends, and if the end of line has
  // open tags, they are not trailable.
  // TODO(crbug.com/1009936): Open tags and trailing space items can interleave,
  // but the current code supports only one trailing space item. Multiple
  // trailing space items and interleaved open/close tags should be supported.
  const InlineItemResults& item_results = line_info->Results();
  for (const InlineItemResult& item_result : base::Reversed(item_results)) {
    DCHECK(item_result.item);
    if (item_result.item->Type() != InlineItem::kOpenTag) {
      unsigned end_index =
          base::checked_cast<unsigned>(&item_result - item_results.begin() + 1);
      if (end_index < item_results.size()) {
        const InlineItemResult& end_item_result = item_results[end_index];
        const InlineItemTextIndex end = end_item_result.Start();
        ResetRewindLoopDetector();
        Rewind(end_index, line_info);
        current_ = end;
        items_data_.AssertOffset(current_.item_index, current_.text_offset);
      }
      break;
    }
  }
}

// Remove trailing collapsible spaces in |line_info|.
// https://drafts.csswg.org/css-text-3/#white-space-phase-2
void LineBreaker::RemoveTrailingCollapsibleSpace(LineInfo* line_info) {
  // Rewind trailing open-tags to wrap before them, except when this line ends
  // with a forced break, including the one implied by block-in-inline.
  if (!is_after_forced_break_)
    RewindTrailingOpenTags(line_info);

  ComputeTrailingCollapsibleSpace(line_info);
  if (!trailing_collapsible_space_.has_value()) {
    return;
  }

  // We have a trailing collapsible space. Remove it.
  InlineItemResult* item_result = trailing_collapsible_space_->item_result;
  position_ -= item_result->inline_size;
  if (scoped_refptr<const ShapeResultView>& collapsed_shape_result =
          trailing_collapsible_space_->collapsed_shape_result) {
    --item_result->text_offset.end;
    item_result->text_offset.AssertNotEmpty();
    item_result->shape_result = collapsed_shape_result;
    item_result->inline_size = item_result->shape_result->SnappedWidth();
    position_ += item_result->inline_size;
  } else {
    // Make it empty, but don't remove. See `HandleEmptyText`.
    item_result->text_offset.end = item_result->text_offset.start;
    item_result->shape_result = nullptr;
    item_result->inline_size = LayoutUnit();
  }
  trailing_collapsible_space_.reset();
  trailing_whitespace_ = WhitespaceState::kCollapsed;
}

// Compute the width of trailing spaces without removing it.
LayoutUnit LineBreaker::TrailingCollapsibleSpaceWidth(LineInfo* line_info) {
  ComputeTrailingCollapsibleSpace(line_info);
  if (!trailing_collapsible_space_.has_value())
    return LayoutUnit();

  // Normally, the width of new_reuslt is smaller, but technically it can be
  // larger. In such case, it means the trailing spaces has negative width.
  InlineItemResult* item_result = trailing_collapsible_space_->item_result;
  if (scoped_refptr<const ShapeResultView>& collapsed_shape_result =
          trailing_collapsible_space_->collapsed_shape_result) {
    return item_result->inline_size - collapsed_shape_result->SnappedWidth();
  }
  return item_result->inline_size;
}

// Find trailing collapsible space if exists. The result is cached to
// |trailing_collapsible_space_|.
void LineBreaker::ComputeTrailingCollapsibleSpace(LineInfo* line_info) {
  if (trailing_whitespace_ == WhitespaceState::kLeading ||
      trailing_whitespace_ == WhitespaceState::kNone ||
      trailing_whitespace_ == WhitespaceState::kCollapsed ||
      trailing_whitespace_ == WhitespaceState::kPreserved) {
    trailing_collapsible_space_.reset();
    return;
  }
  DCHECK(trailing_whitespace_ == WhitespaceState::kUnknown ||
         trailing_whitespace_ == WhitespaceState::kCollapsible);

  trailing_whitespace_ = WhitespaceState::kNone;
  const String& text = Text();
  for (auto& item_result : base::Reversed(*line_info->MutableResults())) {
    DCHECK(item_result.item);
    const InlineItem& item = *item_result.item;
    if (item.EndCollapseType() == InlineItem::kOpaqueToCollapsing) {
      continue;
    }
    if (item.Type() == InlineItem::kText) {
      DCHECK_GT(item_result.EndOffset(), 0u);
      DCHECK(item.Style());
      if (!IsBreakableSpace(text[item_result.EndOffset() - 1]))
        break;
      if (item.Style()->ShouldPreserveWhiteSpaces()) {
        trailing_whitespace_ = WhitespaceState::kPreserved;
        break;
      }
      // |shape_result| is nullptr if this is an overflow because BreakText()
      // uses kNoResultIfOverflow option.
      if (!item_result.shape_result)
        break;

      if (!trailing_collapsible_space_.has_value() ||
          trailing_collapsible_space_->item_result != &item_result) {
        trailing_collapsible_space_.emplace();
        trailing_collapsible_space_->item_result = &item_result;
        if (item_result.EndOffset() - 1 > item_result.StartOffset()) {
          trailing_collapsible_space_->collapsed_shape_result =
              TruncateLineEndResult(*line_info, item_result,
                                    item_result.EndOffset() - 1);
        }
      }
      trailing_whitespace_ = WhitespaceState::kCollapsible;
      return;
    }
    if (item.Type() == InlineItem::kControl) {
      if (item.TextType() == TextItemType::kForcedLineBreak) {
        DCHECK_EQ(text[item.StartOffset()], kNewlineCharacter);
        continue;
      }
      trailing_whitespace_ = WhitespaceState::kPreserved;
      trailing_collapsible_space_.reset();
      return;
    }
    break;
  }

  trailing_collapsible_space_.reset();
}

// |item| is |nullptr| if this is an implicit forced break.
void LineBreaker::HandleForcedLineBreak(const InlineItem* item,
                                        LineInfo* line_info) {
  // Check overflow, because the last item may have overflowed.
  if (HandleOverflowIfNeeded(line_info))
    return;

  if (item) {
    DCHECK_EQ(item->TextType(), TextItemType::kForcedLineBreak);
    DCHECK_EQ(Text()[item->StartOffset()], kNewlineCharacter);

    // Special-code for BR clear elements. If we have floats that extend into
    // subsequent fragmentainers, we cannot get past the floats in the current
    // fragmentainer. If this is the case, and if there's anything on the line
    // before the BR element, add a line break before it, so that we at least
    // attempt to place that part of the line right away. The remaining BR clear
    // element will be placed on a separate line, which we'll push past as many
    // fragmentainers as we need to. Example:
    //
    // <div style="columns:4; column-fill:auto; height:100px;">
    //   <div style="float:left; width:10px; height:350px;"></div>
    //   first column<br clear="all">
    //   fourth column
    // </div>
    //
    // Here we'll create one line box for the first float fragment and the text
    // "first column". We'll later on attempt to create another line box for the
    // BR element, but it will fail in the inline layout algorithm, because it's
    // impossible to clear past the float. We'll retry in the second and third
    // columns, but the float is still in the way. Finally, in the fourth
    // column, we'll add the BR, add clearance, and then create a line for the
    // text "fourth column" past the float.
    //
    // This solution isn't perfect, because of this additional line box for the
    // BR element. We'll push the line box containing the BR to a fragmentainer
    // where it doesn't really belong, and it will take up block space there
    // (this can be observed if the float clearance is less than the height of
    // the line, so that there will be a gap between the bottom of the float and
    // the content that follows). No browser engines currently get BR clearance
    // across fragmentainers right.
    if (constraint_space_.HasBlockFragmentation() && item->GetLayoutObject() &&
        item->GetLayoutObject()->IsBR() &&
        exclusion_space_->NeedsClearancePastFragmentainer(
            item->Style()->Clear(*current_style_))) {
      if (!line_info->Results().empty()) {
        state_ = LineBreakState::kDone;
        return;
      }
    }

    InlineItemResult* item_result = AddItem(*item, line_info);
    item_result->should_create_line_box = true;
    item_result->has_only_trailing_spaces = true;
    item_result->can_break_after = true;
    MoveToNextOf(*item);

    // Include following close tags. The difference is visible when they have
    // margin/border/padding.
    //
    // This is not a defined behavior, but legacy/WebKit do this for preserved
    // newlines and <br>s. Gecko does this only for preserved newlines (but
    // not for <br>s).
    const HeapVector<InlineItem>& items = Items();
    while (current_.item_index < items.size()) {
      const InlineItem& next_item = items[current_.item_index];
      if (next_item.Type() == InlineItem::kCloseTag) {
        HandleCloseTag(next_item, line_info);
        continue;
      }
      if (next_item.Type() == InlineItem::kText && !next_item.Length()) {
        HandleEmptyText(next_item, line_info);
        continue;
      }
      break;
    }
  }

  if (UNLIKELY(HasHyphen()))
    position_ -= RemoveHyphen(line_info->MutableResults());
  is_after_forced_break_ = true;
  line_info->SetHasForcedBreak();
  line_info->SetIsLastLine(true);
  state_ = LineBreakState::kDone;
}

// Measure control items; new lines and tab, that are similar to text, affect
// layout, but do not need shaping/painting.
void LineBreaker::HandleControlItem(const InlineItem& item,
                                    LineInfo* line_info) {
  DCHECK_GE(item.Length(), 1u);
  if (item.TextType() == TextItemType::kForcedLineBreak) {
    HandleForcedLineBreak(&item, line_info);
    return;
  }

  DCHECK_EQ(item.TextType(), TextItemType::kFlowControl);
  UChar character = Text()[item.StartOffset()];
  switch (character) {
    case kTabulationCharacter: {
      DCHECK(item.Style());
      const ComputedStyle& style = *item.Style();
      scoped_refptr<const ShapeResult> shape_result =
          ShapeResult::CreateForTabulationCharacters(
              &style.GetFont(), item.Direction(), style.GetTabSize(), position_,
              item.StartOffset(), item.Length());
      HandleText(item, *shape_result, line_info);
      return;
    }
    case kZeroWidthSpaceCharacter: {
      // <wbr> tag creates break opportunities regardless of auto_wrap.
      InlineItemResult* item_result = AddItem(item, line_info);
      // A generated break opportunity doesn't generate fragments, but we still
      // need to add this for rewind to find this opportunity. This will be
      // discarded in |InlineLayoutAlgorithm| when it generates fragments.
      if (!item.IsGeneratedForLineBreak())
        item_result->should_create_line_box = true;
      item_result->can_break_after = true;
      break;
    }
    case kCarriageReturnCharacter:
    case kFormFeedCharacter:
      // Ignore carriage return and form feed.
      // https://drafts.csswg.org/css-text-3/#white-space-processing
      // https://github.com/w3c/csswg-drafts/issues/855
      HandleEmptyText(item, line_info);
      return;
    default:
      NOTREACHED();
      HandleEmptyText(item, line_info);
      return;
  }
  MoveToNextOf(item);
}

void LineBreaker::HandleBidiControlItem(const InlineItem& item,
                                        LineInfo* line_info) {
  DCHECK_EQ(item.Length(), 1u);

  // Bidi control characters have enter/exit semantics. Handle "enter"
  // characters simialr to open-tag, while "exit" (pop) characters similar to
  // close-tag.
  UChar character = Text()[item.StartOffset()];
  bool is_pop = character == kPopDirectionalIsolateCharacter ||
                character == kPopDirectionalFormattingCharacter;
  InlineItemResults* item_results = line_info->MutableResults();
  if (is_pop) {
    if (!item_results->empty()) {
      InlineItemResult* item_result = AddItem(item, line_info);
      InlineItemResult* last = &(*item_results)[item_results->size() - 2];
      // Honor the last |can_break_after| if it's true, in case it was
      // artificially set to true for break-after-space.
      if (last->can_break_after) {
        item_result->can_break_after = last->can_break_after;
        last->can_break_after = false;
      } else {
        // Otherwise compute from the text. |LazyLineBreakIterator| knows how to
        // break around bidi control characters.
        ComputeCanBreakAfter(item_result, auto_wrap_, break_iterator_);
      }
    } else {
      AddItem(item, line_info);
    }
  } else {
    if (state_ == LineBreakState::kTrailing &&
        CanBreakAfterLast(*item_results)) {
      DCHECK(!line_info->IsLastLine());
      MoveToNextOf(item);
      state_ = LineBreakState::kDone;
      return;
    }
    InlineItemResult* item_result = AddItem(item, line_info);
    DCHECK(!item_result->can_break_after);
  }
  MoveToNextOf(item);
}

void LineBreaker::HandleAtomicInline(const InlineItem& item,
                                     LineInfo* line_info) {
  DCHECK(item.Type() == InlineItem::kAtomicInline ||
         item.Type() == InlineItem::kInitialLetterBox);
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();

  const LayoutUnit remaining_width = RemainingAvailableWidth();
  bool ignore_overflow_if_negative_margin = false;
  if (state_ == LineBreakState::kContinue && remaining_width < 0) {
    const unsigned item_index = current_.item_index;
    DCHECK_EQ(item_index, static_cast<unsigned>(&item - Items().begin()));
    HandleOverflow(line_info);
    if (!line_info->HasOverflow() || item_index != current_.item_index) {
      return;
    }
    // Compute margins if this line overflows. Negative margins can put the
    // position back.
    DCHECK_NE(state_, LineBreakState::kContinue);
    ignore_overflow_if_negative_margin = true;
  }

  // Compute margins before computing overflow, because even when the current
  // position is beyond the end, negative margins can bring this item back to on
  // the current line.
  InlineItemResult* item_result = AddItem(item, line_info);
  item_result->margins =
      ComputeLineMarginsForVisualContainer(constraint_space_, style);
  LayoutUnit inline_margins = item_result->margins.InlineSum();
  if (UNLIKELY(ignore_overflow_if_negative_margin)) {
    DCHECK_LT(remaining_width, 0);
    // The margin isn't negative, or the negative margin isn't large enough to
    // put the position back. Break this line before this item.
    if (inline_margins >= remaining_width) {
      RemoveLastItem(line_info);
      return;
    }
    // This line once overflowed, but the negative margin puts the position
    // back.
    state_ = LineBreakState::kContinue;
    line_info->SetHasOverflow(false);
  }

  // Last item may have ended with a hyphen, because at that point the line may
  // have ended there. Remove it because there are more items.
  if (UNLIKELY(HasHyphen()))
    position_ -= RemoveHyphen(line_info->MutableResults());

  const bool is_initial_letter_box =
      item.Type() == InlineItem::kInitialLetterBox;
  // When we're just computing min/max content sizes, we can skip the full
  // layout and just compute those sizes. On the other hand, for regular
  // layout we need to do the full layout and get the layout result.
  // Doing a full layout for min/max content can also have undesirable
  // side effects when that falls back to legacy layout.
  if (mode_ == LineBreakerMode::kContent || UNLIKELY(is_initial_letter_box)) {
    // If our baseline-source is non-auto use the easier to reason about
    // "default" algorithm type.
    NGBaselineAlgorithmType baseline_algorithm_type =
        style.BaselineSource() == EBaselineSource::kAuto
            ? NGBaselineAlgorithmType::kInlineBlock
            : NGBaselineAlgorithmType::kDefault;

    // https://drafts.csswg.org/css-pseudo-4/#first-text-line
    // > The first line of a table-cell or inline-block cannot be the first
    // > formatted line of an ancestor element.
    item_result->layout_result =
        NGBlockNode(To<LayoutBox>(item.GetLayoutObject()))
            .LayoutAtomicInline(constraint_space_, node_.Style(),
                                /* use_first_line_style */ false,
                                baseline_algorithm_type);

    const auto& physical_box_fragment = To<NGPhysicalBoxFragment>(
        item_result->layout_result->PhysicalFragment());
    item_result->inline_size =
        NGFragment(constraint_space_.GetWritingDirection(),
                   physical_box_fragment)
            .InlineSize();

    if (UNLIKELY(is_initial_letter_box) &&
        ShouldApplyInlineKerning(physical_box_fragment)) {
      // Apply "Inline Kerning" to the initial letter box[1].
      // [1] https://drafts.csswg.org/css-inline/#initial-letter-inline-position
      const LineBoxStrut side_bearing =
          ComputeNegativeSideBearings(physical_box_fragment);
      if (IsLtr(base_direction_)) {
        item_result->margins.inline_start += side_bearing.inline_start;
        inline_margins += side_bearing.inline_start;
      } else {
        item_result->margins.inline_end += side_bearing.inline_end;
        inline_margins += side_bearing.inline_end;
      }
    }

    item_result->inline_size += inline_margins;
  } else {
    DCHECK(mode_ == LineBreakerMode::kMaxContent ||
           mode_ == LineBreakerMode::kMinContent);
    ComputeMinMaxContentSizeForBlockChild(item, item_result);
  }

  item_result->should_create_line_box = true;
  item_result->can_break_after = CanBreakAfterAtomicInline(item);

  position_ += item_result->inline_size;

  if (item.IsRubyColumn()) {
    AnnotationOverhang overhang = GetOverhang(*item_result);
    if (overhang.end > LayoutUnit()) {
      item_result->pending_end_overhang = overhang.end;
      maybe_have_end_overhang_ = true;
    }

    if (CanApplyStartOverhang(*line_info, overhang.start)) {
      DCHECK_EQ(item_result->margins.inline_start, LayoutUnit());
      item_result->margins.inline_start = -overhang.start;
      item_result->inline_size -= overhang.start;
      position_ -= overhang.start;
    }
  }

  trailing_whitespace_ = WhitespaceState::kNone;
  MoveToNextOf(item);
}

void LineBreaker::ComputeMinMaxContentSizeForBlockChild(
    const InlineItem& item,
    InlineItemResult* item_result) {
  DCHECK(mode_ == LineBreakerMode::kMaxContent ||
         mode_ == LineBreakerMode::kMinContent);
  if (mode_ == LineBreakerMode::kMaxContent && max_size_cache_) {
    const unsigned item_index =
        base::checked_cast<unsigned>(&item - Items().begin());
    item_result->inline_size = (*max_size_cache_)[item_index];
    return;
  }

  DCHECK(mode_ == LineBreakerMode::kMinContent || !max_size_cache_);
  NGBlockNode child(To<LayoutBox>(item.GetLayoutObject()));

  NGMinMaxConstraintSpaceBuilder builder(constraint_space_, node_.Style(),
                                         child, /* is_new_fc */ true);
  builder.SetAvailableBlockSize(constraint_space_.AvailableSize().block_size);
  builder.SetPercentageResolutionBlockSize(
      constraint_space_.PercentageResolutionBlockSize());
  builder.SetReplacedPercentageResolutionBlockSize(
      constraint_space_.ReplacedPercentageResolutionBlockSize());
  const auto space = builder.ToConstraintSpace();

  const MinMaxSizesResult result =
      ComputeMinAndMaxContentContribution(node_.Style(), child, space);
  const LayoutUnit inline_margins = item_result->margins.InlineSum();
  if (mode_ == LineBreakerMode::kMinContent) {
    item_result->inline_size = result.sizes.min_size + inline_margins;
    if (depends_on_block_constraints_out_)
      *depends_on_block_constraints_out_ |= result.depends_on_block_constraints;
    if (max_size_cache_) {
      if (max_size_cache_->empty())
        max_size_cache_->resize(Items().size());
      const unsigned item_index =
          base::checked_cast<unsigned>(&item - Items().begin());
      (*max_size_cache_)[item_index] = result.sizes.max_size + inline_margins;
    }
    return;
  }

  DCHECK(mode_ == LineBreakerMode::kMaxContent && !max_size_cache_);
  item_result->inline_size = result.sizes.max_size + inline_margins;
}

void LineBreaker::HandleBlockInInline(
    const InlineItem& item,
    const NGBlockBreakToken* block_break_token,
    LineInfo* line_info) {
  DCHECK_EQ(item.Type(), InlineItem::kBlockInInline);
  DCHECK(!block_break_token || block_break_token->InputNode().GetLayoutBox() ==
                                   item.GetLayoutObject());

  if (!line_info->Results().empty()) {
    // If there were any items, force a line break before this item.
    force_non_empty_if_last_line_ = false;
    HandleForcedLineBreak(nullptr, line_info);
    return;
  }

  InlineItemResult* item_result = AddItem(item, line_info);
  bool move_past_block = true;
  if (mode_ == LineBreakerMode::kContent) {
    // The exclusion spaces *must* match. If they don't we'll have an incorrect
    // layout (as it will potentially won't consider some preceeding floats).
    // Move the derived geometry for performance.
    DCHECK(*exclusion_space_ == constraint_space_.GetExclusionSpace());
    constraint_space_.GetExclusionSpace().MoveAndUpdateDerivedGeometry(
        *exclusion_space_);

    NGBlockNode block_node(To<LayoutBox>(item.GetLayoutObject()));
    const NGColumnSpannerPath* spanner_path_for_child =
        FollowColumnSpannerPath(column_spanner_path_, block_node);
    const NGLayoutResult* layout_result =
        block_node.Layout(constraint_space_, block_break_token,
                          /* early_break */ nullptr, spanner_path_for_child);
    line_info->SetBlockInInlineLayoutResult(layout_result);

    // Early exit if the layout didn't succeed.
    if (layout_result->Status() != NGLayoutResult::kSuccess) {
      state_ = LineBreakState::kDone;
      return;
    }

    const NGPhysicalFragment& fragment = layout_result->PhysicalFragment();
    item_result->inline_size =
        NGFragment(constraint_space_.GetWritingDirection(), fragment)
            .InlineSize();

    item_result->should_create_line_box = !layout_result->IsSelfCollapsing();
    item_result->layout_result = layout_result;

    if (const auto* outgoing_block_break_token = To<NGBlockBreakToken>(
            layout_result->PhysicalFragment().BreakToken())) {
      // The block broke inside. If the block itself fits, but some content
      // inside overflowed, we now need to enter a parallel flow, i.e. resume
      // the block-in-inline in the next fragmentainer, but continue layout of
      // any actual inline content after the block-in- inline in the current
      // fragmentainer.
      if (outgoing_block_break_token->IsAtBlockEnd()) {
        const auto* parallel_token =
            InlineBreakToken::CreateForParallelBlockFlow(
                node_, current_, *outgoing_block_break_token);
        line_info->PropagateParallelFlowBreakToken(parallel_token);
      } else {
        // The block-in-inline broke inside, and it's still in the same flow.
        resume_block_in_inline_in_same_flow_ = true;
        move_past_block = false;
      }
    }
  } else {
    DCHECK(mode_ == LineBreakerMode::kMaxContent ||
           mode_ == LineBreakerMode::kMinContent);
    ComputeMinMaxContentSizeForBlockChild(item, item_result);
  }

  position_ += item_result->inline_size;
  line_info->SetIsBlockInInline();
  line_info->SetHasForcedBreak();
  is_after_forced_break_ = true;
  trailing_whitespace_ = WhitespaceState::kNone;

  // If there's no break inside the block, or if the break inside the block is
  // for a parallel flow, proceed to the next item for the next line.
  if (move_past_block) {
    MoveToNextOf(item);
  }
  state_ = LineBreakState::kDone;
}

// Figure out if the float should be pushed after the current line. This
// should only be considered if we're not resuming the float, after having
// broken inside or before it in the previous fragmentainer. Otherwise we must
// attempt to place it.
bool LineBreaker::ShouldPushFloatAfterLine(
    NGUnpositionedFloat* unpositioned_float,
    LineInfo* line_info) {
  if (unpositioned_float->token) {
    return false;
  }

  LayoutUnit inline_margin_size =
      ComputeMarginBoxInlineSizeForUnpositionedFloat(unpositioned_float);

  LayoutUnit used_size = position_ + inline_margin_size +
                         ComputeFloatAncestorInlineEndSize(
                             constraint_space_, Items(), current_.item_index);
  bool can_fit_float =
      used_size <= line_opportunity_.AvailableFloatInlineSize().AddEpsilon();
  if (!can_fit_float) {
    // Floats need to know the current line width to determine whether to put it
    // into the current line or to the next line. Trailing spaces will be
    // removed if this line breaks here because they should be collapsed across
    // floats, but they are still included in the current line position at this
    // point. Exclude it when computing whether this float can fit or not.
    can_fit_float = used_size - TrailingCollapsibleSpaceWidth(line_info) <=
                    line_opportunity_.AvailableFloatInlineSize().AddEpsilon();
  }

  LayoutUnit bfc_block_offset =
      unpositioned_float->origin_bfc_offset.block_offset;

  // The float should be positioned after the current line if:
  //  - It can't fit within the non-shape area. (Assuming the current position
  //    also is strictly within the non-shape area).
  //  - It will be moved down due to block-start edge alignment.
  //  - It will be moved down due to clearance.
  //  - An earlier float has been pushed to the next fragmentainer.
  return !can_fit_float ||
         exclusion_space_->LastFloatBlockStart() > bfc_block_offset ||
         exclusion_space_->ClearanceOffset(unpositioned_float->ClearType(
             constraint_space_.Direction())) > bfc_block_offset;
}

// Performs layout and positions a float.
//
// If there is a known available_width (e.g. something has resolved the
// container BFC block offset) it will attempt to position the float on the
// current line.
// Additionally updates the available_width for the line as the float has
// (probably) consumed space.
//
// If the float is too wide *or* we already have UnpositionedFloats we add it
// as an UnpositionedFloat. This should be positioned *immediately* after we
// are done with the current line.
// We have this check if there are already UnpositionedFloats as we aren't
// allowed to position a float "above" another float which has come before us
// in the document.
void LineBreaker::HandleFloat(const InlineItem& item,
                              const NGBlockBreakToken* float_break_token,
                              LineInfo* line_info) {
  // When rewind occurs, an item may be handled multiple times.
  // Since floats are put into a separate list, avoid handling same floats
  // twice.
  // Ideally rewind can take floats out of floats list, but the difference is
  // sutble compared to the complexity.
  //
  // Additionally, we need to skip floats if we're retrying a line after a
  // fragmentainer break. In that case the floats associated with this line will
  // already have been processed.
  InlineItemResult* item_result = AddItem(item, line_info);
  auto index_before_float = current_;
  item_result->can_break_after = auto_wrap_;
  MoveToNextOf(item);

  // If we are currently computing our min/max-content size, simply append the
  // unpositioned floats to |LineInfo| and abort.
  if (mode_ != LineBreakerMode::kContent) {
    return;
  }

  // Make sure we populate the positioned_float inside the |item_result|.
  if (current_.item_index <= leading_floats_.handled_index &&
      !leading_floats_.floats.empty()) {
    DCHECK_LT(leading_floats_index_, leading_floats_.floats.size());
    item_result->positioned_float =
        leading_floats_.floats[leading_floats_index_++];

    // Don't break after leading floats if indented.
    if (position_ != 0)
      item_result->can_break_after = false;
    return;
  }

  LayoutUnit bfc_block_offset = line_opportunity_.bfc_block_offset;
  NGUnpositionedFloat unpositioned_float(
      NGBlockNode(To<LayoutBox>(item.GetLayoutObject())), float_break_token,
      constraint_space_.AvailableSize(),
      constraint_space_.PercentageResolutionSize(),
      constraint_space_.ReplacedPercentageResolutionSize(),
      {constraint_space_.GetBfcOffset().line_offset, bfc_block_offset},
      constraint_space_, node_.Style());

  bool float_after_line =
      ShouldPushFloatAfterLine(&unpositioned_float, line_info);

  // Check if we already have a pending float. That's because a float cannot be
  // higher than any block or floated box generated before.
  if (HasUnpositionedFloats(line_info->Results()) || float_after_line) {
    item_result->has_unpositioned_floats = true;
    return;
  }

  item_result->positioned_float =
      PositionFloat(&unpositioned_float, exclusion_space_);

  if (constraint_space_.HasBlockFragmentation()) {
    if (const auto* break_token = item_result->positioned_float->BreakToken()) {
      const auto* parallel_token = InlineBreakToken::CreateForParallelBlockFlow(
          node_, index_before_float, *break_token);
      line_info->PropagateParallelFlowBreakToken(parallel_token);
      if (item_result->positioned_float->minimum_space_shortage) {
        line_info->PropagateMinimumSpaceShortage(
            item_result->positioned_float->minimum_space_shortage);
      }
      if (break_token->IsBreakBefore()) {
        return;
      }
    }
  }

  LayoutOpportunity opportunity = exclusion_space_->FindLayoutOpportunity(
      {constraint_space_.GetBfcOffset().line_offset, bfc_block_offset},
      constraint_space_.AvailableSize().inline_size);

  DCHECK_EQ(bfc_block_offset, opportunity.rect.BlockStartOffset());

  line_opportunity_ = opportunity.ComputeLineLayoutOpportunity(
      constraint_space_, line_opportunity_.line_block_size, LayoutUnit());
  UpdateAvailableWidth();

  DCHECK_GE(AvailableWidth(), LayoutUnit());
}

void LineBreaker::HandleInitialLetter(const InlineItem& item,
                                      LineInfo* line_info) {
  // TODO(crbug.com/1276900): We should check behavior when line breaking
  // after initial letter box.
  HandleAtomicInline(item, line_info);
}

void LineBreaker::HandleOutOfFlowPositioned(const InlineItem& item,
                                            LineInfo* line_info) {
  DCHECK_EQ(item.Type(), InlineItem::kOutOfFlowPositioned);
  InlineItemResult* item_result = AddItem(item, line_info);

  // Break opportunity after OOF is not well-defined nor interoperable. Using
  // |kObjectReplacementCharacter|, except when this is a leading OOF, seems to
  // produce reasonable and interoperable results in common cases.
  DCHECK(!item_result->can_break_after);
  if (item_result->should_create_line_box)
    ComputeCanBreakAfter(item_result, auto_wrap_, break_iterator_);

  MoveToNextOf(item);
}

bool LineBreaker::ComputeOpenTagResult(
    const InlineItem& item,
    const NGConstraintSpace& constraint_space,
    bool is_in_svg_text,
    InlineItemResult* item_result) {
  DCHECK_EQ(item.Type(), InlineItem::kOpenTag);
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
  if (!is_in_svg_text && item.ShouldCreateBoxFragment() &&
      (style.HasBorder() || style.MayHavePadding() || style.MayHaveMargin())) {
    item_result->borders = ComputeLineBorders(style);
    item_result->padding = ComputeLinePadding(constraint_space, style);
    item_result->margins = ComputeLineMarginsForSelf(constraint_space, style);
    item_result->inline_size = item_result->margins.inline_start +
                               item_result->borders.inline_start +
                               item_result->padding.inline_start;
    return true;
  }
  return false;
}

void LineBreaker::HandleOpenTag(const InlineItem& item, LineInfo* line_info) {
  DCHECK_EQ(item.Type(), InlineItem::kOpenTag);

  InlineItemResult* item_result = AddItem(item, line_info);
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
  if (ComputeOpenTagResult(item, constraint_space_, is_svg_text_,
                           item_result)) {
    // Negative margins on open tags may bring the position back. Update
    // |state_| if that happens.
    if (UNLIKELY(item_result->inline_size < 0 &&
                 state_ == LineBreakState::kTrailing)) {
      LayoutUnit available_width = AvailableWidthToFit();
      if (position_ > available_width &&
          position_ + item_result->inline_size <= available_width) {
        state_ = LineBreakState::kContinue;
      }
    }

    position_ += item_result->inline_size;

    // While the spec defines "non-zero margins, padding, or borders" prevents
    // line boxes to be zero-height, tests indicate that only inline direction
    // of them do so. See should_create_line_box_.
    // Force to create a box, because such inline boxes affect line heights.
    if (!item_result->should_create_line_box && !item.IsEmptyItem())
      item_result->should_create_line_box = true;
  }

  if (UNLIKELY(style.BoxDecorationBreak() == EBoxDecorationBreak::kClone)) {
    // Compute even when no margins/borders/padding to ensure correct counting.
    has_cloned_box_decorations_ = true;
    disable_score_line_break_ = true;
    ++cloned_box_decorations_count_;
    cloned_box_decorations_end_size_ += item_result->margins.inline_end +
                                        item_result->borders.inline_end +
                                        item_result->padding.inline_end;
  }

  bool was_auto_wrap = auto_wrap_;
  SetCurrentStyle(style);
  MoveToNextOf(item);

  DCHECK(!item_result->can_break_after);
  const InlineItemResults& item_results = line_info->Results();
  if (UNLIKELY(!was_auto_wrap && auto_wrap_ && item_results.size() >= 2)) {
    if (IsPreviousItemOfType(InlineItem::kText)) {
      ComputeCanBreakAfter(std::prev(item_result), auto_wrap_, break_iterator_);
    }
  }
}

void LineBreaker::HandleCloseTag(const InlineItem& item, LineInfo* line_info) {
  InlineItemResult* item_result = AddItem(item, line_info);

  if (!is_svg_text_) {
    DCHECK(item.Style());
    const ComputedStyle& style = *item.Style();
    item_result->inline_size = ComputeInlineEndSize(constraint_space_, &style);
    position_ += item_result->inline_size;

    if (!item_result->should_create_line_box && !item.IsEmptyItem())
      item_result->should_create_line_box = true;

    if (UNLIKELY(style.BoxDecorationBreak() == EBoxDecorationBreak::kClone)) {
      DCHECK_GT(cloned_box_decorations_count_, 0u);
      --cloned_box_decorations_count_;
      DCHECK_GE(cloned_box_decorations_end_size_, item_result->inline_size);
      cloned_box_decorations_end_size_ -= item_result->inline_size;
    }
  }
  DCHECK(item.GetLayoutObject() && item.GetLayoutObject()->Parent());
  bool was_auto_wrap = auto_wrap_;
  SetCurrentStyle(item.GetLayoutObject()->Parent()->StyleRef());
  MoveToNextOf(item);

  // If the line can break after the previous item, prohibit it and allow break
  // after this close tag instead. Even when the close tag has "nowrap", break
  // after it is allowed if the line is breakable after the previous item.
  const InlineItemResults& item_results = line_info->Results();
  if (item_results.size() >= 2) {
    InlineItemResult* last = std::prev(item_result);
    if (UNLIKELY(IsA<LayoutTextCombine>(last->item->GetLayoutObject()))) {
      // |can_break_after| for close tag should be as same as text-combine box.
      // See "text-combine-upright-break-inside-001a.html"
      // e.g. A<tcy style="white-space: pre">x y</tcy>B
      item_result->can_break_after = last->can_break_after;
      return;
    }
    if (last->can_break_after) {
      // A break opportunity before a close tag always propagates to after the
      // close tag.
      item_result->can_break_after = true;
      last->can_break_after = false;
      return;
    }
    if (was_auto_wrap) {
      // We can break before a breakable space if we either:
      //   a) allow breaking before a white space, or
      //   b) the break point is preceded by another breakable space.
      // TODO(abotella): What if the following breakable space is after an
      // open tag which has a different white-space value?
      bool preceded_by_breakable_space =
          item_result->EndOffset() > 0 &&
          IsBreakableSpace(Text()[item_result->EndOffset() - 1]);
      item_result->can_break_after =
          IsBreakableSpace(Text()[item_result->EndOffset()]) &&
          (!current_style_->ShouldBreakOnlyAfterWhiteSpace() ||
           preceded_by_breakable_space);
      return;
    }
    if (auto_wrap_ && !IsBreakableSpace(Text()[item_result->EndOffset() - 1]))
      ComputeCanBreakAfter(item_result, auto_wrap_, break_iterator_);
  }
}

// Handles when the last item overflows.
// At this point, item_results does not fit into the current line, and there
// are no break opportunities in item_results.back().
void LineBreaker::HandleOverflow(LineInfo* line_info) {
  // Compute the width needing to rewind. When |width_to_rewind| goes negative,
  // items can fit within the line.
  LayoutUnit available_width = AvailableWidthToFit();
  LayoutUnit width_to_rewind = position_ - available_width;
  DCHECK_GT(width_to_rewind, 0);

  // Keep track of the shortest break opportunity.
  unsigned break_before = 0;

  // True if there is at least one item that has `break-word`.
  bool has_break_anywhere_if_overflow = break_anywhere_if_overflow_;

  // Save the hyphenation states before we may make changes.
  InlineItemResults* item_results = line_info->MutableResults();
  absl::optional<wtf_size_t> hyphen_index_before = hyphen_index_;
  if (UNLIKELY(HasHyphen()))
    position_ -= RemoveHyphen(item_results);

  // Search for a break opportunity that can fit.
  for (unsigned i = item_results->size(); i;) {
    InlineItemResult* item_result = &(*item_results)[--i];
    has_break_anywhere_if_overflow |= item_result->break_anywhere_if_overflow;

    // Try to break after this item.
    if (i < item_results->size() - 1 && item_result->can_break_after) {
      if (width_to_rewind <= 0) {
        position_ = available_width + width_to_rewind;
        RewindOverflow(i + 1, line_info);
        return;
      }
      break_before = i + 1;
    }

    // Compute the position after this item was removed entirely.
    width_to_rewind -= item_result->inline_size;

    // Try next if still does not fit.
    if (width_to_rewind > 0)
      continue;

    DCHECK(item_result->item);
    const InlineItem& item = *item_result->item;
    if (item.Type() == InlineItem::kText) {
      if (!item_result->Length()) {
        // Empty text items are trailable, see `HandleEmptyText`.
        continue;
      }
      DCHECK(item_result->shape_result ||
             (item_result->break_anywhere_if_overflow &&
              !override_break_anywhere_) ||
             // |HandleTextForFastMinContent| can produce an item without
             // |ShapeResult|. In this case, it is not breakable.
             (mode_ == LineBreakerMode::kMinContent &&
              !item_result->may_break_inside));
      // If space is available, and if this text is breakable, part of the text
      // may fit. Try to break this item.
      if (width_to_rewind < 0 && item_result->may_break_inside) {
        const LayoutUnit item_available_width = -width_to_rewind;
        // Make sure the available width is smaller than the current width. The
        // break point must not be at the end when e.g., the text fits but its
        // right margin does not or following items do not.
        const LayoutUnit min_available_width = item_result->inline_size - 1;
        // If |inline_size| is zero (e.g., `font-size: 0`), |BreakText| cannot
        // make it shorter. Take the previous break opportunity.
        if (UNLIKELY(min_available_width <= 0)) {
          if (BreakTextAtPreviousBreakOpportunity(item_result)) {
            RewindOverflow(i + 1, line_info);
            return;
          }
          continue;
        }
        const ComputedStyle* was_current_style = current_style_;
        SetCurrentStyle(*item.Style());
        const InlineItemResult item_result_before = *item_result;
        BreakText(item_result, item, *item.TextShapeResult(),
                  std::min(item_available_width, min_available_width),
                  item_available_width, line_info);
        DCHECK_LE(item_result->EndOffset(), item_result_before.EndOffset());
#if DCHECK_IS_ON()
        item_result->CheckConsistency(true);
#endif

        // If BreakText() changed this item small enough to fit, break here.
        if (item_result->can_break_after &&
            item_result->inline_size <= item_available_width &&
            item_result->EndOffset() < item_result_before.EndOffset()) {
          DCHECK_LT(item_result->EndOffset(), item.EndOffset());

          // If this is the last item, adjust it to accommodate the change.
          const unsigned new_end = i + 1;
          CHECK_LE(new_end, item_results->size());
          if (new_end == item_results->size()) {
            position_ =
                available_width + width_to_rewind + item_result->inline_size;
            DCHECK_EQ(position_, line_info->ComputeWidth());
            current_ = item_result->End();
            items_data_.AssertOffset(current_);
            HandleTrailingSpaces(item, line_info);
            return;
          }

          state_ = LineBreakState::kTrailing;
          Rewind(new_end, line_info);
          return;
        }

        // Failed to break to fit. Restore to the original state.
        if (UNLIKELY(HasHyphen()))
          RemoveHyphen(item_results);
        *item_result = std::move(item_result_before);
        SetCurrentStyle(*was_current_style);
      }
    }
  }

  // Reaching here means that the rewind point was not found.

  if (break_iterator_.BreakType() == LineBreakType::kPhrase &&
      !disable_phrase_ && mode_ == LineBreakerMode::kContent) {
    // If the phrase line break overflowed, retry with the normal line break.
    disable_phrase_ = true;
    RetryAfterOverflow(line_info, item_results);
    return;
  }

  if (!override_break_anywhere_ && has_break_anywhere_if_overflow) {
    // Overflow occurred but `overflow-wrap` is set. Change the break type and
    // retry the line breaking.
    override_break_anywhere_ = true;
    RetryAfterOverflow(line_info, item_results);
    return;
  }

  // Let this line overflow.
  line_info->SetHasOverflow();

  // TODO(kojii): `ScoreLineBreaker::ComputeScores` gets confused if there're
  // overflowing lines. Disable the score line break for now. E.g.:
  //   css2.1/t1601-c547-indent-01-d.html
  //   virtual/text-antialias/international/bdi-neutral-wrapped.html
  disable_score_line_break_ = true;

  // Restore the hyphenation states to before the loop if needed.
  DCHECK(!HasHyphen());
  if (UNLIKELY(hyphen_index_before))
    position_ += AddHyphen(item_results, *hyphen_index_before);

  // If there was a break opportunity, the overflow should stop there.
  if (break_before) {
    RewindOverflow(break_before, line_info);
    return;
  }

  if (CanBreakAfterLast(*item_results)) {
    state_ = LineBreakState::kTrailing;
    return;
  }

  // No break opportunities. Break at the earliest break opportunity.
  DCHECK(base::ranges::all_of(*item_results,
                              [](const InlineItemResult& item_result) {
                                return !item_result.can_break_after;
                              }));
  state_ = LineBreakState::kOverflow;
}

void LineBreaker::RetryAfterOverflow(LineInfo* line_info,
                                     InlineItemResults* item_results) {
  // `ScoreLineBreaker` doesn't support multi-pass line breaking.
  disable_score_line_break_ = true;

  state_ = LineBreakState::kContinue;

  // Rewind all items.
  //
  // Also `SetCurrentStyle` forcibly, because the retry uses different
  // conditions such as `override_break_anywhere_`.
  //
  // TODO(kojii): Not all items need to rewind, but such case is rare and
  // rewinding all items simplifes the code.
  if (!item_results->empty()) {
    SetCurrentStyleForce(ComputeCurrentStyle(0, line_info));
    Rewind(0, line_info);
  } else {
    SetCurrentStyleForce(*current_style_);
  }
  ResetRewindLoopDetector();
}

// Rewind to |new_end| on overflow. If trailable items follow at |new_end|, they
// are included (not rewound).
void LineBreaker::RewindOverflow(unsigned new_end, LineInfo* line_info) {
  const HeapVector<InlineItem>& items = Items();
  const InlineItemResults& item_results = line_info->Results();
  DCHECK_LT(new_end, item_results.size());

  unsigned open_tag_count = 0;
  const String& text = Text();
  for (unsigned index = new_end; index < item_results.size(); index++) {
    const InlineItemResult& item_result = item_results[index];
    DCHECK(item_result.item);
    const InlineItem& item = *item_result.item;
    if (item.Type() == InlineItem::kText) {
      // Text items are trailable if they start with trailable spaces.
      if (!item_result.Length()) {
        // Empty text items are trailable, see `HandleEmptyText`.
        continue;
      }
      if (item_result.shape_result ||  // kNoResultIfOverflow if 'break-word'
          (break_anywhere_if_overflow_ && !override_break_anywhere_)) {
        DCHECK(item.Style());
        const ComputedStyle& style = *item.Style();
        if (style.ShouldWrapLine() && !style.ShouldBreakSpaces() &&
            IsBreakableSpace(text[item_result.StartOffset()])) {
          // If all characters are trailable spaces, check the next item.
          if (item_result.shape_result &&
              IsAllBreakableSpaces(text, item_result.StartOffset() + 1,
                                   item_result.EndOffset())) {
            continue;
          }
          // If this item starts with spaces followed by non-space characters,
          // the line should break after the spaces. Rewind to before this item.
          //
          // After the rewind, we want to |HandleTrailingSpaces| in this |item|,
          // but |Rewind| may have failed when we have floats. Set the |state_|
          // to |kTrailing| and let the next |HandleText| to handle this.
          state_ = LineBreakState::kTrailing;
          Rewind(index, line_info);
          return;
        }
      }
    } else if (item.Type() == InlineItem::kControl) {
      // All control characters except newline are trailable if auto_wrap. We
      // should not have rewound if there was a newline, so safe to assume all
      // controls are trailable.
      DCHECK_NE(text[item_result.StartOffset()], kNewlineCharacter);
      DCHECK(item.Style());
      const ComputedStyle& style = *item.Style();
      if (style.ShouldWrapLine() && !style.ShouldBreakSpaces()) {
        continue;
      }
    } else if (item.Type() == InlineItem::kOpenTag) {
      // Open tags are ambiguous. This open tag is not trailable:
      //   <span>text
      // but these are trailable:
      //   <span> text
      //   <span></span>text
      //   <span> </span>text
      // Count the nest-level and mark where the nest-level was 0.
      if (!open_tag_count)
        new_end = index;
      open_tag_count++;
      continue;
    } else if (item.Type() == InlineItem::kCloseTag) {
      if (open_tag_count > 0)
        open_tag_count--;
      continue;
    } else if (IsTrailableItemType(item.Type())) {
      continue;
    }

    // Found a non-trailable item. Rewind to before the item, or to before the
    // open tag if the nest-level is not zero.
    if (open_tag_count)
      index = new_end;
    state_ = LineBreakState::kDone;
    DCHECK(!line_info->IsLastLine());
    Rewind(index, line_info);
    return;
  }

  // The open tag turned out to be non-trailable if the nest-level is not zero.
  // Rewind to before the open tag.
  if (open_tag_count) {
    state_ = LineBreakState::kDone;
    DCHECK(!line_info->IsLastLine());
    Rewind(new_end, line_info);
    return;
  }

  // All items are trailable. Done without rewinding.
  trailing_whitespace_ = WhitespaceState::kUnknown;
  position_ = line_info->ComputeWidth();
  state_ = LineBreakState::kDone;
  DCHECK(!line_info->IsLastLine());
  if (current_.item_index == items.size()) {
    line_info->SetIsLastLine(true);
  }
}

void LineBreaker::Rewind(unsigned new_end, LineInfo* line_info) {
  InlineItemResults& item_results = *line_info->MutableResults();
  DCHECK_LT(new_end, item_results.size());
  if (last_rewind_) {
    // Detect rewind-loop. If we're trying to rewind to the same index twice,
    // we're in the infinite loop.
    if (current_.item_index == last_rewind_->from_item_index &&
        new_end == last_rewind_->to_index) {
      NOTREACHED();
      state_ = LineBreakState::kDone;
      return;
    }
    last_rewind_.emplace(RewindIndex{current_.item_index, new_end});
  }

  // Avoid rewinding floats if possible. They will be added back anyway while
  // processing trailing items even when zero available width. Also this saves
  // most cases where our support for rewinding positioned floats is not great
  // yet (see below.)
  while (item_results[new_end].item->Type() == InlineItem::kFloating) {
    // We assume floats can break after, or this may cause an infinite loop.
    DCHECK(item_results[new_end].can_break_after);
    ++new_end;
    if (new_end == item_results.size()) {
      if (UNLIKELY(!hyphen_index_ && has_any_hyphens_))
        RestoreLastHyphen(&item_results);
      position_ = line_info->ComputeWidth();
      return;
    }
  }

  // Because floats are added to |positioned_floats_| or |unpositioned_floats_|,
  // rewinding them needs to remove from these lists too.
  for (unsigned i = item_results.size(); i > new_end;) {
    InlineItemResult& rewind = item_results[--i];
    if (rewind.positioned_float) {
      // We assume floats can break after, or this may cause an infinite loop.
      DCHECK(rewind.can_break_after);
      // TODO(kojii): We do not have mechanism to remove once positioned floats
      // yet, and that rewinding them may lay it out twice. For now, prohibit
      // rewinding positioned floats. This may results in incorrect layout, but
      // still better than rewinding them.
      new_end = i + 1;
      if (new_end == item_results.size()) {
        if (UNLIKELY(!hyphen_index_ && has_any_hyphens_))
          RestoreLastHyphen(&item_results);
        position_ = line_info->ComputeWidth();
        return;
      }
      break;
    }
  }

  if (new_end) {
    // Use |results[new_end - 1].end_offset| because it may have been truncated
    // and may not be equal to |results[new_end].start_offset|.
    MoveToNextOf(item_results[new_end - 1]);
    trailing_whitespace_ = WhitespaceState::kUnknown;
    // When space item is followed by empty text, we will break line at empty
    // text. See http://crbug.com/1104534
    // Example:
    //   [0] kOpeNTag 0-0 <i>
    //   [1] kText 0-10 "012345679"
    //   [2] kOpenTag 10-10 <b> <= |item_results[new_end - 1]|
    //   [3] kText 10-10 ""     <= |current_.item_index|
    //   [4] kText 10-11 " "
    //   [5] kCloseTag 11-11 <b>
    //   [6] kText 11-13 "ab"
    //   [7] kCloseTag 13-13 <i>
    // Note: We can have multiple empty |LayoutText| by ::first-letter, nested
    // <q>, Text.splitText(), etc.
    const HeapVector<InlineItem>& items = Items();
    while (current_.item_index < items.size() &&
           items[current_.item_index].Type() == InlineItem::kText &&
           !items[current_.item_index].Length()) {
      HandleEmptyText(items[current_.item_index], line_info);
    }
  } else {
    // Rewinding all items.
    current_ = line_info->Start();
    trailing_whitespace_ = WhitespaceState::kLeading;
    maybe_have_end_overhang_ = false;
  }
  SetCurrentStyle(ComputeCurrentStyle(new_end, line_info));

  item_results.Shrink(new_end);

  trailing_collapsible_space_.reset();
  if (UNLIKELY(hyphen_index_ && *hyphen_index_ >= new_end))
    hyphen_index_.reset();
  if (UNLIKELY(!hyphen_index_ && has_any_hyphens_))
    RestoreLastHyphen(&item_results);
  position_ = line_info->ComputeWidth();
  if (UNLIKELY(has_cloned_box_decorations_))
    RecalcClonedBoxDecorations();
}

// Returns the style to use for |item_result_index|. Normally when handling
// items sequentially, the current style is updated on open/close tag. When
// rewinding, this function computes the style for the specified item.
const ComputedStyle& LineBreaker::ComputeCurrentStyle(
    unsigned item_result_index,
    LineInfo* line_info) const {
  const InlineItemResults& item_results = line_info->Results();

  // Use the current item if it can compute the current style.
  const InlineItem* item = item_results[item_result_index].item;
  DCHECK(item);
  if (item->Type() == InlineItem::kText ||
      item->Type() == InlineItem::kCloseTag) {
    DCHECK(item->Style());
    return *item->Style();
  }

  // Otherwise look back an item that can compute the current style.
  while (item_result_index) {
    item = item_results[--item_result_index].item;
    if (item->Type() == InlineItem::kText ||
        item->Type() == InlineItem::kOpenTag) {
      DCHECK(item->Style());
      return *item->Style();
    }
    if (item->Type() == InlineItem::kCloseTag) {
      return item->GetLayoutObject()->Parent()->StyleRef();
    }
  }

  // Use the style at the beginning of the line if no items are available.
  if (break_token_ && break_token_->Style())
    return *break_token_->Style();
  return line_info->LineStyle();
}

void LineBreaker::SetCurrentStyle(const ComputedStyle& style) {
  if (&style == current_style_) {
#if EXPENSIVE_DCHECKS_ARE_ON()
    // Check that cache fields are already setup correctly.
    DCHECK_EQ(auto_wrap_, ShouldAutoWrap(style));
    if (auto_wrap_) {
      DCHECK_EQ(break_iterator_.IsSoftHyphenEnabled(),
                style.GetHyphens() != Hyphens::kNone &&
                    (disable_phrase_ ||
                     style.WordBreak() != EWordBreak::kAutoPhrase));
      DCHECK_EQ(break_iterator_.Locale(), style.GetFontDescription().Locale());
    }
    ShapeResultSpacing<String> spacing(spacing_.Text(), is_svg_text_);
    spacing.SetSpacing(style.GetFont().GetFontDescription());
    DCHECK_EQ(spacing.LetterSpacing(), spacing_.LetterSpacing());
    DCHECK_EQ(spacing.WordSpacing(), spacing_.WordSpacing());
#endif  //  EXPENSIVE_DCHECKS_ARE_ON()
    return;
  }
  SetCurrentStyleForce(style);
}

void LineBreaker::SetCurrentStyleForce(const ComputedStyle& style) {
  current_style_ = &style;

  const FontDescription& font_description = style.GetFontDescription();
  spacing_.SetSpacing(font_description);

  auto_wrap_ = ShouldAutoWrap(style);
  if (auto_wrap_) {
    DCHECK(!is_text_combine_);
    break_iterator_.SetLocale(font_description.Locale());
    Hyphens hyphens = style.GetHyphens();
    const LineBreak line_break = style.GetLineBreak();
    if (UNLIKELY(line_break == LineBreak::kAnywhere)) {
      break_iterator_.SetStrictness(LineBreakStrictness::kDefault);
      break_iterator_.SetBreakType(LineBreakType::kBreakCharacter);
      break_anywhere_if_overflow_ = false;
    } else {
      break_iterator_.SetStrictness(StrictnessFromLineBreak(line_break));
      LineBreakType line_break_type;
      switch (style.WordBreak()) {
        case EWordBreak::kNormal:
          line_break_type = LineBreakType::kNormal;
          break_anywhere_if_overflow_ = false;
          break;
        case EWordBreak::kBreakAll:
          line_break_type = LineBreakType::kBreakAll;
          break_anywhere_if_overflow_ = false;
          break;
        case EWordBreak::kBreakWord:
          line_break_type = LineBreakType::kNormal;
          break_anywhere_if_overflow_ = true;
          break;
        case EWordBreak::kKeepAll:
          line_break_type = LineBreakType::kKeepAll;
          break_anywhere_if_overflow_ = false;
          break;
        case EWordBreak::kAutoPhrase:
          DCHECK(RuntimeEnabledFeatures::CSSPhraseLineBreakEnabled());
          if (UNLIKELY(disable_phrase_)) {
            line_break_type = LineBreakType::kNormal;
          } else {
            line_break_type = LineBreakType::kPhrase;
            hyphens = Hyphens::kNone;
            UseCounter::Count(GetDocument(), WebFeature::kLineBreakPhrase);
          }
          break_anywhere_if_overflow_ = false;
          break;
      }
      if (!break_anywhere_if_overflow_) {
        // `overflow-wrap: anywhere` affects both layout and min-content, while
        // `break-word` affects layout but not min-content.
        const EOverflowWrap overflow_wrap = style.OverflowWrap();
        break_anywhere_if_overflow_ =
            overflow_wrap == EOverflowWrap::kAnywhere ||
            (overflow_wrap == EOverflowWrap::kBreakWord &&
             mode_ == LineBreakerMode::kContent);
      }
      if (UNLIKELY(override_break_anywhere_ && break_anywhere_if_overflow_)) {
        line_break_type = LineBreakType::kBreakCharacter;
      }
      break_iterator_.SetBreakType(line_break_type);
    }

    if (UNLIKELY(hyphens == Hyphens::kNone)) {
      break_iterator_.EnableSoftHyphen(false);
      hyphenation_ = nullptr;
    } else {
      break_iterator_.EnableSoftHyphen(true);
      hyphenation_ = style.GetHyphenationWithLimits();
    }

    if (style.ShouldBreakSpaces()) {
      break_iterator_.SetBreakSpace(BreakSpaceType::kAfterEverySpace);
      disable_score_line_break_ = true;
    } else {
      break_iterator_.SetBreakSpace(BreakSpaceType::kAfterSpaceRun);
    }
  }
}

bool LineBreaker::IsPreviousItemOfType(InlineItem::InlineItemType type) {
  return current_.item_index > 0
             ? Items().at(current_.item_index - 1).Type() == type
             : false;
}

void LineBreaker::MoveToNextOf(const InlineItem& item) {
  current_.text_offset = item.EndOffset();
  current_.item_index++;
#if DCHECK_IS_ON()
  const HeapVector<InlineItem>& items = Items();
  if (current_.item_index < items.size()) {
    items[current_.item_index].AssertOffset(current_.text_offset);
  } else {
    DCHECK_EQ(current_.text_offset, Text().length());
  }
#endif
}

void LineBreaker::MoveToNextOf(const InlineItemResult& item_result) {
  current_ = item_result.End();
  DCHECK(item_result.item);
  if (current_.text_offset == item_result.item->EndOffset()) {
    current_.item_index++;
  }
}

const InlineBreakToken* LineBreaker::CreateBreakToken(
    const LineInfo& line_info) {
#if DCHECK_IS_ON()
  DCHECK(!has_considered_creating_break_token_);
  has_considered_creating_break_token_ = true;
#endif

  DCHECK(current_style_);
  const HeapVector<InlineItem>& items = Items();
  DCHECK_LE(current_.item_index, items.size());
  // If we have reached the end, create no break token.
  if (current_.item_index >= items.size()) {
    return nullptr;
  }

  const NGBlockBreakToken* sub_break_token = nullptr;
  if (resume_block_in_inline_in_same_flow_) {
    const auto* block_in_inline = line_info.BlockInInlineLayoutResult();
    DCHECK(block_in_inline);
    if (UNLIKELY(block_in_inline->Status() != NGLayoutResult::kSuccess)) {
      return nullptr;
    }
    // Look for a break token inside the block-in-inline, so that we can add it
    // to the inline break token that we're about to create.
    const auto& block_in_inline_fragment =
        To<NGPhysicalBoxFragment>(block_in_inline->PhysicalFragment());
    sub_break_token = block_in_inline_fragment.BreakToken();
  }

  DCHECK_EQ(line_info.HasForcedBreak(), is_after_forced_break_);
  unsigned flags =
      (is_after_forced_break_ ? InlineBreakToken::kIsForcedBreak : 0) |
      (line_info.UseFirstLineStyle() ? InlineBreakToken::kUseFirstLineStyle
                                     : 0) |
      (cloned_box_decorations_count_
           ? InlineBreakToken::kHasClonedBoxDecorations
           : 0);

  return InlineBreakToken::Create(node_, current_style_, current_, flags,
                                  sub_break_token);
}

}  // namespace blink
